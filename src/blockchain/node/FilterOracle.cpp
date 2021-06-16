// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                      // IWYU pragma: associated
#include "1_Internal.hpp"                    // IWYU pragma: associated
#include "blockchain/node/FilterOracle.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "blockchain/DownloadTask.hpp"
#include "blockchain/node/filteroracle/BlockIndexer.hpp"
#include "blockchain/node/filteroracle/FilterCheckpoints.hpp"
#include "blockchain/node/filteroracle/FilterDownloader.hpp"
#include "blockchain/node/filteroracle/HeaderDownloader.hpp"
#include "internal/api/Api.hpp"
#include "internal/api/network/Network.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Bytes.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/ThreadPool.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/FilterType.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/block/bitcoin/Block.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/blockchain/sync/Block.hpp"
#include "opentxs/network/blockchain/sync/Data.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/socket/Socket.hpp"
#include "util/ScopeGuard.hpp"

#define OT_METHOD "opentxs::blockchain::node::implementation::FilterOracle::"

using ReturnType = opentxs::blockchain::node::implementation::FilterOracle;

namespace opentxs::factory
{
auto BlockchainFilterOracle(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::internal::HeaderOracle& header,
    const blockchain::node::internal::BlockOracle& block,
    const blockchain::node::internal::FilterDatabase& database,
    const blockchain::Type type,
    const std::string& shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::FilterOracle>
{
    return std::make_unique<ReturnType>(
        api, network, config, node, header, block, database, type, shutdown);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::implementation
{
FilterOracle::FilterOracle(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const internal::Config& config,
    const internal::Manager& node,
    const internal::HeaderOracle& header,
    const internal::BlockOracle& block,
    const internal::FilterDatabase& database,
    const blockchain::Type chain,
    const std::string& shutdown) noexcept
    : internal::FilterOracle()
    , api_(api)
    , node_(node)
    , header_(header)
    , database_(database)
    , filter_notifier_(network.FilterUpdate())
    , chain_(chain)
    , default_type_([&] {
        if (config.generate_cfilters_ || config.use_sync_server_) {

            return filter::Type::ES;
        }

        return blockchain::internal::DefaultFilter(chain_);
    }())
    , lock_()
    , new_filters_([&] {
        auto socket = api_.Network().ZeroMQ().PublishSocket();
        auto started = socket->Start(
            api_.Endpoints().InternalBlockchainFilterUpdated(chain_));

        OT_ASSERT(started);

        return socket;
    }())
    , cb_([this](const auto type, const auto& pos) {
        auto lock = rLock{lock_};
        new_tip(lock, type, pos);
    })
    , thread_pool_([&] {
        auto socket = api_.Network().ZeroMQ().PushSocket(
            zmq::socket::Socket::Direction::Connect);
        const auto started = socket->Start(api_.ThreadPool().Endpoint());

        OT_ASSERT(started);

        return socket;
    }())
    , filter_downloader_([&]() -> std::unique_ptr<FilterDownloader> {
        if (config.download_cfilters_) {
            return std::make_unique<FilterDownloader>(
                api,
                database_,
                header_,
                node_,
                chain,
                default_type_,
                shutdown,
                cb_);
        } else {
            return {};
        }
    }())
    , header_downloader_([&]() -> std::unique_ptr<HeaderDownloader> {
        if (config.download_cfilters_) {
            return std::make_unique<HeaderDownloader>(
                api,
                database_,
                header_,
                node_,
                *filter_downloader_,
                chain,
                default_type_,
                shutdown,
                [&](const auto& position, const auto& header) {
                    return compare_header_to_checkpoint(position, header);
                });
        } else {
            return {};
        }
    }())
    , block_indexer_([&]() -> std::unique_ptr<BlockIndexer> {
        if (config.generate_cfilters_) {
            return std::make_unique<BlockIndexer>(
                api,
                database_,
                header_,
                block,
                node_,
                *this,
                chain,
                default_type_,
                shutdown,
                cb_,
                thread_pool_);
        } else {
            return {};
        }
    }())
    , last_sync_progress_()
    , outstanding_jobs_()
    , running_(true)
{
    OT_ASSERT(cb_);

    compare_tips_to_header_chain();
    compare_tips_to_checkpoint();
}

auto FilterOracle::compare_header_to_checkpoint(
    const block::Position& block,
    const filter::Header& receivedHeader) noexcept -> block::Position
{
    const auto& cp = filter_checkpoints_.at(chain_);
    const auto height = block.first;

    if (auto it = cp.find(height); cp.end() != it) {
        const auto& bytes = it->second.at(default_type_);
        const auto expectedHeader =
            api_.Factory().Data(ReadView{bytes.data(), bytes.size()});

        if (expectedHeader == receivedHeader) {
            LogNormal(DisplayString(chain_))(" filter header at height ")(
                height)(" verified against checkpoint")
                .Flush();

            return block;
        } else {
            OT_ASSERT(cp.begin() != it);

            std::advance(it, -1);
            const auto rollback =
                block::Position{it->first, header_.BestHash(it->first)};
            LogNormal(DisplayString(chain_))(" filter header at height ")(
                height)(" does not match checkpoint. Resetting to previous "
                        "checkpoint at height ")(rollback.first)
                .Flush();

            return rollback;
        }
    }

    return block;
}

auto FilterOracle::compare_tips_to_checkpoint() noexcept -> void
{
    const auto& cp = filter_checkpoints_.at(chain_);
    const auto headerTip = database_.FilterHeaderTip(default_type_);
    auto checkPosition{headerTip};
    auto changed{false};

    for (auto i{cp.crbegin()}; i != cp.crend(); ++i) {
        const auto& cpHeight = i->first;

        if (cpHeight > checkPosition.first) { continue; }

        checkPosition = block::Position{cpHeight, header_.BestHash(cpHeight)};
        const auto existingHeader = database_.LoadFilterHeader(
            default_type_, checkPosition.second->Bytes());

        try {
            const auto& cpHeader = i->second.at(default_type_);
            const auto cpBytes =
                api_.Factory().Data(ReadView{cpHeader.data(), cpHeader.size()});

            if (existingHeader == cpBytes) { break; }

            changed = true;
        } catch (...) {
            break;
        }
    }

    if (changed) {
        LogNormal(DisplayString(chain_))(
            " filter header chain did not match checkpoint. Resetting to last "
            "known good position")
            .Flush();
        reset_tips_to(default_type_, headerTip, checkPosition, changed);
    } else {
        LogVerbose(DisplayString(chain_))(
            " filter header chain matched checkpoint")
            .Flush();
    }
}

auto FilterOracle::compare_tips_to_header_chain() noexcept -> bool
{
    const auto current = database_.FilterHeaderTip(default_type_);
    const auto [parent, best] = header_.CommonParent(current);

    if ((parent.first == current.first) && (parent.second == current.second)) {
        LogVerbose(DisplayString(chain_))(
            " filter header chain is following the best chain")
            .Flush();

        return false;
    }

    LogNormal(DisplayString(chain_))(
        " filter header chain is following a sibling chain. Resetting to "
        "common ancestor at height ")(parent.first)
        .Flush();
    reset_tips_to(default_type_, current, parent);

    return true;
}

auto FilterOracle::GetFilterJob() const noexcept -> CfilterJob
{
    auto lock = rLock{lock_};

    if (filter_downloader_) {

        return filter_downloader_->NextBatch();
    } else {

        return {};
    }
}

auto FilterOracle::GetHeaderJob() const noexcept -> CfheaderJob
{
    auto lock = rLock{lock_};

    if (header_downloader_) {

        return header_downloader_->NextBatch();
    } else {

        return {};
    }
}

auto FilterOracle::Heartbeat() const noexcept -> void
{
    auto lock = rLock{lock_};

    if (filter_downloader_) { filter_downloader_->Heartbeat(); }
    if (header_downloader_) { header_downloader_->Heartbeat(); }
    if (block_indexer_) { block_indexer_->Heartbeat(); }

    constexpr auto limit = std::chrono::seconds{5};

    if ((Clock::now() - last_sync_progress_) > limit) {
        new_tip(lock, default_type_, database_.FilterTip(default_type_));
    }
}

auto FilterOracle::LoadFilterOrResetTip(
    const filter::Type type,
    const block::Position& position) const noexcept
    -> std::unique_ptr<const node::GCS>
{
    auto output = LoadFilter(type, position.second);

    if (output) { return output; }

    const auto height = position.first;

    OT_ASSERT(0 < height);

    const auto parent = height - 1;
    const auto hash = header_.BestHash(parent);

    OT_ASSERT(false == hash->empty());

    reset_tips_to(type, block::Position{parent, hash}, false, true);

    return {};
}

auto FilterOracle::new_tip(
    const rLock&,
    const filter::Type type,
    const block::Position& tip) const noexcept -> void
{
    last_sync_progress_ = Clock::now();
    {
        auto work = MakeWork(api_, OT_ZMQ_NEW_FILTER_SIGNAL);
        work->AddFrame(type);
        work->AddFrame(tip.first);
        work->AddFrame(tip.second);
        new_filters_->Send(work);
    }
    {
        auto work = MakeWork(api_, WorkType::BlockchainNewFilter);
        work->AddFrame(chain_);
        work->AddFrame(type);
        work->AddFrame(tip.first);
        work->AddFrame(tip.second);
        filter_notifier_.Send(work);
    }
}

auto FilterOracle::ProcessBlock(
    const block::bitcoin::Block& block) const noexcept -> bool
{
    const auto& id = block.ID();
    const auto& header = block.Header();
    auto filters = std::vector<internal::FilterDatabase::Filter>{};
    auto headers = std::vector<internal::FilterDatabase::Header>{};
    const auto& pGCS =
        filters.emplace_back(id.Bytes(), process_block(default_type_, block))
            .second;

    if (false == bool(pGCS)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to calculate ")(
            DisplayString(chain_))(" cfilter")
            .Flush();

        return false;
    }

    const auto& gcs = *pGCS;
    const auto previousHeader =
        LoadFilterHeader(default_type_, header.ParentHash());

    if (previousHeader->empty()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": failed to load previous")(
            DisplayString(chain_))(" cfheader")
            .Flush();

        return false;
    }

    const auto filterHash = gcs.Hash();
    const auto& cfheader = std::get<1>(headers.emplace_back(
        id, gcs.Header(previousHeader->Bytes()), filterHash->Bytes()));

    if (cfheader->empty()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": failed to calculate ")(
            DisplayString(chain_))(" cfheader")
            .Flush();

        return false;
    }

    const auto position = make_blank<block::Position>::value(api_);
    const auto stored =
        database_.StoreFilters(default_type_, headers, filters, position);

    if (stored) {

        return true;
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Database error ").Flush();

        return false;
    }
}

auto FilterOracle::ProcessBlock(BlockIndexerData& data) const noexcept -> void
{
    auto post = ScopeGuard{[&] { --data.job_counter_; }};
    auto& task = data.incoming_data_;
    const auto& [height, block] = task.position_;

    try {
        LogTrace(OT_METHOD)(__FUNCTION__)(": Calculating filter for ")(
            DisplayString(chain_))(" block at height ")(height)
            .Flush();
        auto& [blockHashView, pGCS] = data.filter_data_;
        auto& [blockHash, filterHeader, filterHashView] = data.header_data_;
        blockHash = block;
        blockHashView = blockHash->Bytes();
        const auto pBlock = task.data_.get();

        if (false == bool(pBlock)) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to load ")(
                DisplayString(chain_))(" block #")(height)
                .Flush();

            throw std::runtime_error(
                std::string{"failed to load block "} + blockHash->asHex());
        }

        pGCS = process_block(data.type_, *pBlock);

        if (false == bool(pGCS)) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to instantiate ")(
                DisplayString(chain_))(" cfilter #")(height)
                .Flush();

            throw std::runtime_error("Failed to instantiate gcs");
        }

        const auto& gcs = *pGCS;
        data.filter_hash_ = gcs.Hash();
        filterHashView = data.filter_hash_->Bytes();
        auto& previous = task.previous_;
        constexpr auto limit = std::chrono::minutes{1};
        using State = std::future_status;

        if (auto status = previous.wait_for(limit); State::ready != status) {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Timeout waiting for previous ")(DisplayString(chain_))(
                " cfheader #")(height - 1)
                .Flush();

            throw std::runtime_error("timeout");
        }

        filterHeader = gcs.Header(previous.get()->Bytes());

        if (filterHeader->empty()) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": failed to calculate ")(
                DisplayString(chain_))(" cfheader #")(height)
                .Flush();

            throw std::runtime_error("Failed to calculate cfheader");
        }

        LogTrace(OT_METHOD)(__FUNCTION__)(
            ": Finished calculating cfheader and cfilter for ")(
            DisplayString(chain_))(" block at height ")(height)
            .Flush();
        task.process(filter::pHeader{filterHeader});
    } catch (...) {
        task.process(std::current_exception());
    }
}

auto FilterOracle::ProcessSyncData(
    const filter::Type type,
    const block::Position& tip,
    const std::vector<internal::FilterDatabase::Filter>& cfilters,
    const std::vector<internal::FilterDatabase::Header>& cfheaders)
    const noexcept -> void
{
    const auto stored = database_.StoreFilters(type, cfheaders, cfilters, tip);

    if (stored) {
        LogDetail(DisplayString(chain_))(
            " cfheader and cfilter chain updated to height ")(tip.first)
            .Flush();
        cb_(type, tip);
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Database error ").Flush();
    }
}

auto FilterOracle::process_block(
    const filter::Type filterType,
    const block::bitcoin::Block& block) const noexcept
    -> std::unique_ptr<const node::GCS>
{
    const auto& id = block.ID();
    const auto params = blockchain::internal::GetFilterParams(filterType);
    const auto elements = [&] {
        const auto input = block.ExtractElements(filterType);
        auto output = std::vector<OTData>{};
        std::transform(
            input.begin(),
            input.end(),
            std::back_inserter(output),
            [&](const auto& element) -> OTData {
                return api_.Factory().Data(reader(element));
            });

        return output;
    }();

    return factory::GCS(
        api_,
        params.first,
        params.second,
        blockchain::internal::BlockHashToFilterKey(id.Bytes()),
        elements);
}

auto FilterOracle::reset_tips_to(
    const filter::Type type,
    const block::Position& position,
    const std::optional<bool> resetHeader,
    const std::optional<bool> resetfilter) const noexcept -> bool
{
    return reset_tips_to(
        type,
        database_.FilterHeaderTip(default_type_),
        database_.FilterTip(default_type_),
        position,
        resetHeader,
        resetfilter);
}

auto FilterOracle::reset_tips_to(
    const filter::Type type,
    const block::Position& headerTip,
    const block::Position& position,
    const std::optional<bool> resetHeader) const noexcept -> bool
{
    return reset_tips_to(
        type,
        headerTip,
        database_.FilterTip(default_type_),
        position,
        resetHeader);
}

auto FilterOracle::reset_tips_to(
    const filter::Type type,
    const block::Position& headerTip,
    const block::Position& filterTip,
    const block::Position& position,
    std::optional<bool> resetHeader,
    std::optional<bool> resetfilter) const noexcept -> bool
{
    auto counter{0};

    if (false == resetHeader.has_value()) {
        resetHeader = headerTip > position;
    }

    if (false == resetfilter.has_value()) {
        resetfilter = filterTip > position;
    }

    OT_ASSERT(resetHeader.has_value());
    OT_ASSERT(resetfilter.has_value());

    auto lock = rLock{lock_};
    using Future = std::shared_future<filter::pHeader>;
    auto previous = [&]() -> Future {
        const auto& block = header_.LoadHeader(position.second);

        OT_ASSERT(block);

        auto promise = std::promise<filter::pHeader>{};
        promise.set_value(database_.LoadFilterHeader(
            default_type_, block->ParentHash().Bytes()));

        return promise.get_future();
    }();
    auto resetBlock{false};

    if (resetHeader.value()) {
        if (header_downloader_) {
            header_downloader_->Reset(position, Future{previous});
        }

        resetBlock = true;
        ++counter;
    }

    if (resetfilter.value()) {
        if (filter_downloader_) {
            filter_downloader_->Reset(position, Future{previous});
        }

        resetBlock = true;
        ++counter;
    }

    if (resetBlock && block_indexer_) {
        block_indexer_->Reset(position, std::move(previous));
    }

    return 0 < counter;
}

auto FilterOracle::Shutdown() noexcept -> void
{
    running_ = false;

    auto lock = rLock{lock_};

    if (header_downloader_) { header_downloader_.reset(); }

    if (filter_downloader_) { filter_downloader_.reset(); }

    if (block_indexer_) { block_indexer_.reset(); }
}

auto FilterOracle::Start() noexcept -> void
{
    auto lock = rLock{lock_};

    if (header_downloader_) { header_downloader_->Start(); }

    if (filter_downloader_) { filter_downloader_->Start(); }

    if (block_indexer_) { block_indexer_->Start(); }
}

FilterOracle::~FilterOracle() { Shutdown(); }
}  // namespace opentxs::blockchain::node::implementation

namespace opentxs::blockchain::node::internal
{
auto FilterOracle::ProcessThreadPool(const zmq::Message& in) noexcept -> void
{
    const auto header = in.Header();
    const auto body = in.Body();

    if (1 > header.size()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid message").Flush();

        OT_FAIL;
    }

    if (2 > body.size()) {
        LogOutput("opentxs::blockchain::node::internal:FilterOracle::")(
            __FUNCTION__)(": Invalid message")
            .Flush();

        OT_FAIL;
    }

    using Imp = implementation::FilterOracle;

    const auto* pFilterOracle =
        reinterpret_cast<Imp*>(body.at(0).as<std::uintptr_t>());

    OT_ASSERT(nullptr != pFilterOracle);

    const auto& filterOracle = *pFilterOracle;
    using Work = api::internal::ThreadPool::Work;
    const auto work = [&] {
        try {

            return header.at(header.size() - 1u).as<Work>();
        } catch (...) {

            OT_FAIL;
        }
    }();

    switch (work) {
        case Work::CalculateBlockFilters: {
            auto* pData = reinterpret_cast<Imp::BlockIndexerData*>(
                body.at(1).as<std::uintptr_t>());

            OT_ASSERT(nullptr != pData);

            auto& data = *pData;
            filterOracle.ProcessBlock(data);
        } break;
        default: {
            OT_FAIL;
        }
    }
}
}  // namespace opentxs::blockchain::node::internal
