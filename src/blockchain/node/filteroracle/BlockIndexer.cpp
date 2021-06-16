// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/filteroracle/BlockIndexer.hpp"  // IWYU pragma: associated

#include <chrono>
#include <exception>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include "blockchain/DownloadManager.hpp"
#include "blockchain/DownloadTask.hpp"
#include "internal/api/Api.hpp"
#include "opentxs/Bytes.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/block/bitcoin/Block.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "util/JobCounter.hpp"
#include "util/ScopeGuard.hpp"

#define OT_METHOD                                                              \
    "opentxs::blockchain::node::implementation::FilterOracle::BlockIndexer:"   \
    ":"

namespace opentxs::blockchain::node::implementation
{
FilterOracle::BlockIndexer::BlockIndexer(
    const api::Core& api,
    const internal::FilterDatabase& db,
    const internal::HeaderOracle& header,
    const internal::BlockOracle& block,
    const internal::Manager& node,
    FilterOracle& parent,
    const blockchain::Type chain,
    const filter::Type type,
    const std::string& shutdown,
    const NotifyCallback& notify,
    zmq::socket::Push& threadPool) noexcept
    : BlockDM(
          [&] { return db.FilterTip(type); }(),
          [&] {
              auto promise = std::promise<filter::pHeader>{};
              const auto tip = db.FilterTip(type);
              promise.set_value(db.LoadFilterHeader(type, tip.second->Bytes()));

              return Finished{promise.get_future()};
          }(),
          "filter",
          2000,
          1000)
    , BlockWorker(api, std::chrono::milliseconds{20})
    , db_(db)
    , header_(header)
    , block_(block)
    , node_(node)
    , parent_(parent)
    , chain_(chain)
    , type_(type)
    , notify_(notify)
    , thread_pool_(threadPool)
    , job_counter_()
{
    init_executor(
        {shutdown, api_.Endpoints().InternalBlockchainBlockUpdated(chain_)});
}

auto FilterOracle::BlockIndexer::batch_size(const std::size_t in) const noexcept
    -> std::size_t
{
    if (in < 10) {

        return 1;
    } else if (in < 100) {

        return 10;
    } else if (in < 1000) {

        return 100;
    } else {

        return 1000;
    }
}

auto FilterOracle::BlockIndexer::download() noexcept -> void
{
    auto work = NextBatch();
    constexpr auto none = std::chrono::seconds{0};

    for (const auto& task : work.data_) {
        const auto& hash = task->position_.second;
        auto future = block_.LoadBitcoin(hash);

        if (std::future_status::ready == future.wait_for(none)) {
            auto block = future.get();

            if (!block) { return; }  // Might occur during shutdown

            OT_ASSERT(block->ID() == hash);

            task->download(std::move(block));
        }
    }
}

auto FilterOracle::BlockIndexer::pipeline(const zmq::Message& in) noexcept
    -> void
{
    if (false == running_.get()) { return; }

    const auto body = in.Body();

    OT_ASSERT(1 <= body.size());

    using Work = FilterOracle::Work;
    const auto work = [&] {
        try {

            return body.at(0).as<Work>();
        } catch (...) {

            OT_FAIL;
        }
    }();

    switch (work) {
        case Work::shutdown: {
            shutdown(shutdown_promise_);
        } break;
        case Work::heartbeat: {
            if (dm_enabled()) { process_position(block_.Tip()); }

            run_if_enabled();
        } break;
        case Work::full_block: {
            if (dm_enabled()) { process_position(in); }

            run_if_enabled();
        } break;
        case Work::statemachine: {
            download();
            run_if_enabled();
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto FilterOracle::BlockIndexer::process_position(
    const zmq::Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(body.size() > 2);

    process_position(Position{
        body.at(1).as<block::Height>(), api_.Factory().Data(body.at(2))});
}

auto FilterOracle::BlockIndexer::process_position(const Position& pos) noexcept
    -> void
{
    const auto current = known();
    auto compare{current};
    LogTrace(OT_METHOD)(__FUNCTION__)(":  Current position: ")(current.first)(
        ",")(current.second->asHex())
        .Flush();
    LogTrace(OT_METHOD)(__FUNCTION__)(": Incoming position: ")(pos.first)(",")(
        pos.second->asHex())
        .Flush();
    auto hashes = decltype(header_.Ancestors(current, pos)){};
    auto prior = Previous{std::nullopt};
    auto searching{true};

    while (searching) {
        try {
            hashes = header_.Ancestors(compare, pos, 2000u);

            OT_ASSERT(0 < hashes.size());
        } catch (const std::exception& e) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();
            reset_to_genesis();

            return;
        }

        auto postcondition = ScopeGuard{[&] { hashes.erase(hashes.begin()); }};
        auto& first = hashes.front();
        LogTrace(OT_METHOD)(__FUNCTION__)(":          Ancestor: ")(first.first)(
            ",")(first.second->asHex())
            .Flush();

        if (first == pos) { return; }

        if (first == current) {
            searching = false;
            break;
        }

        auto header = db_.LoadFilterHeader(type_, first.second->Bytes());
        const auto filter = db_.LoadFilter(type_, first.second->Bytes());

        if (header->empty()) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Missing cfheader for block ")(
                first.first)(",")(first.second->asHex())
                .Flush();
        }

        if (!filter) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Missing cfilter for block ")(
                first.first)(",")(first.second->asHex())
                .Flush();
        }

        if (filter && (false == header->empty())) {
            auto promise = std::promise<filter::pHeader>{};
            promise.set_value(std::move(header));
            prior.emplace(std::move(first), promise.get_future());
            searching = false;
            break;
        }

        OT_ASSERT(0 < first.first);

        const auto height = first.first - 1;
        compare = block::Position{height, header_.BestHash(height)};
    }

    update_position(std::move(hashes), type_, std::move(prior));
}

auto FilterOracle::BlockIndexer::queue_processing(
    DownloadedData&& data) noexcept -> void
{
    if (0u == data.size()) { return; }

    auto filters = std::vector<internal::FilterDatabase::Filter>{};
    auto headers = std::vector<internal::FilterDatabase::Header>{};
    auto cache = std::vector<BlockIndexerData>{};
    const auto& tip = data.back();
    const auto cores = std::thread::hardware_concurrency();

    {
        auto jobCounter = job_counter_.Allocate();
        const auto count{data.size()};

        if (0 == data.size()) { return; }

        filters.reserve(count);
        headers.reserve(count);
        cache.reserve(count);
        static const auto blank = api_.Factory().Data();
        static const auto blankView = ReadView{};

        for (const auto& task : data) {
            if (false == running_.get()) { return; }

            auto& filter = filters.emplace_back(blankView, nullptr);
            auto& header = headers.emplace_back(blank, blank, blankView);
            auto& job = cache.emplace_back(
                blank, *task, type_, filter, header, jobCounter);

            if (2 < cores) {
                send_to_thread_pool(job);
            } else {
                parent_.ProcessBlock(job);
            }
        }
    }

    try {
        tip->output_.get();
        const auto stored =
            db_.StoreFilters(type_, headers, filters, tip->position_);

        if (false == stored) {
            throw std::runtime_error(DisplayString(chain_) + " database error");
        }
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        for (auto& task : data) { task->process(std::current_exception()); }
    }
}

auto FilterOracle::BlockIndexer::reset_to_genesis() noexcept -> void
{
    LogOutput(OT_METHOD)(__FUNCTION__)(": Performing full reset").Flush();
    static const auto genesis =
        block::Position{0, header_.GenesisBlockHash(chain_)};
    auto promise = std::promise<filter::pHeader>{};
    promise.set_value(db_.LoadFilterHeader(type_, genesis.second->Bytes()));
    Reset(genesis, promise.get_future());
}

auto FilterOracle::BlockIndexer::shutdown(std::promise<void>& promise) noexcept
    -> void
{
    if (running_->Off()) {
        try {
            promise.set_value();
        } catch (...) {
        }
    }
}

auto FilterOracle::BlockIndexer::send_to_thread_pool(
    BlockIndexerData& job) noexcept -> void
{
    auto& jobCounter = job.job_counter_;

    while (running_.get() && jobCounter.limited()) {
        Sleep(std::chrono::microseconds(1));
    }

    if (false == running_.get()) { return; }

    using Pool = api::internal::ThreadPool;
    auto work = Pool::MakeWork(
        api_.Network().ZeroMQ(), value(Pool::Work::CalculateBlockFilters));
    work->AddFrame(reinterpret_cast<std::uintptr_t>(&parent_));
    work->AddFrame(reinterpret_cast<std::uintptr_t>(&job));
    thread_pool_.Send(work);
    ++jobCounter;
}

auto FilterOracle::BlockIndexer::update_tip(
    const Position& position,
    const filter::pHeader&) const noexcept -> void
{
    auto saved = db_.SetFilterTip(type_, position);

    OT_ASSERT(saved);

    saved = db_.SetFilterHeaderTip(type_, position);

    OT_ASSERT(saved);

    LogDetail(DisplayString(chain_))(
        " cfheader and cfilter chain updated to height ")(position.first)
        .Flush();
    notify_(type_, position);
}

FilterOracle::BlockIndexer::~BlockIndexer() { stop_worker().get(); }
}  // namespace opentxs::blockchain::node::implementation
