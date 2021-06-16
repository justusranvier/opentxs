// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                           // IWYU pragma: associated
#include "1_Internal.hpp"                         // IWYU pragma: associated
#include "blockchain/node/base/SyncPipeline.hpp"  // IWYU pragma: associated

#include <map>
#include <mutex>
#include <vector>

#include "core/Worker.hpp"
#include "internal/api/Api.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/block/Block.hpp"
#include "internal/blockchain/block/bitcoin/Bitcoin.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/network/blockchain/sync/Base.hpp"
#include "opentxs/network/blockchain/sync/Data.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/socket/Pair.hpp"

namespace zmq = opentxs::network::zeromq;

namespace opentxs::blockchain::node::base
{
struct SyncPipeline::Imp {
    auto Init() const noexcept -> void
    {
        const auto tip = filter_.FilterTip(filter_.DefaultType());
        auto msg = MakeWork(api_, OTZMQWorkType{OT_ZMQ_INTERNAL_SIGNAL + 2});
        msg->AddFrame(tip.first);
        msg->AddFrame(tip.second);
        socket_->Send(msg);
    }

    Imp(const api::internal::Core& api,
        const node::internal::Manager& parent,
        node::internal::HeaderOracle& header,
        const node::internal::FilterOracle& filter,
        const std::string& endpoint) noexcept
        : api_(api)
        , zmq_(api_.Network().ZeroMQ())
        , parent_(parent)
        , header_(header)
        , filter_(filter)
        , lock_()
        , counter_(0)
        , tasks_()
        , cache_()
        , stage_1_(zmq::ListenCallback::Factory([&](auto& in) { stage_1(in); }))
        , socket_([&] {
            auto out = zmq_.PairSocket(stage_1_, endpoint);

            return out;
        }())
        , stage_2_(zmq_.Pipeline(api_, [&](auto& in) { stage_2(in); }))
        , stage_3_(zmq_.Pipeline(api_, [&](auto& in) { stage_3(in); }))
        , stage_4_(zmq_.Pipeline(api_, [&](auto& in) { stage_4(in); }))
        , stage_5_(zmq_.Pipeline(api_, [&](auto& in) { stage_5(in); }))
        , stage_6_(zmq_.Pipeline(api_, [&](auto& in) { stage_6(in); }))
        , stage_7_(zmq_.Pipeline(api_, [&](auto& in) { stage_7(in); }))
    {
    }

private:
    using Base = network::blockchain::sync::Base;
    using Index = long long int;

    struct Work {
        using Header = std::unique_ptr<block::Header>;

        const Index index_;
        const std::unique_ptr<const Base> base_;
        std::vector<Header> block_headers_;
        std::vector<block::pHash> block_hashes_;
        std::vector<filter::pHash> filter_hashes_;
        std::vector<internal::FilterDatabase::Filter> filters_;
        std::vector<internal::FilterDatabase::Header> filter_headers_;
        std::optional<block::Position> tip_;

        auto Tip() noexcept -> const block::Position&
        {
            if (false == tip_.has_value()) {
                const auto& data = base_->asData().Blocks();

                OT_ASSERT(0 < data.size());
                OT_ASSERT(0 < block_hashes_.size());

                tip_.emplace(data.back().Height(), block_hashes_.back());
            }

            return tip_.value();
        }

        Work(network::blockchain::sync::Base* data, Index index) noexcept
            : index_(index)
            , base_(data)
            , block_headers_()
            , block_hashes_()
            , filter_hashes_()
            , filters_()
            , filter_headers_()
            , tip_(std::nullopt)
        {
            OT_ASSERT(base_);

            const auto count = base_->asData().Blocks().size();
            block_headers_.reserve(count);
            block_hashes_.reserve(count);
            filter_hashes_.reserve(count);
            filters_.reserve(count);
            filter_headers_.reserve(count);
        }

    private:
        Work() = delete;
        Work(const Work&) = delete;
        Work(Work&&) = delete;
        auto operator=(const Work&) -> Work& = delete;
        auto operator=(Work&&) -> Work& = delete;
    };
    struct SyncCache {
        using CfilterFuture = std::shared_future<filter::pHeader>;
        using BlockFuture = std::shared_future<block::pHash>;

        auto Get(block::Height height) const noexcept
            -> std::optional<BlockFuture>
        {
            auto lock = Lock{lock_};

            try {

                return hashes_.at(height);
            } catch (...) {

                return std::nullopt;
            }
        }
        auto Get(const ReadView blockHash) const noexcept(false)
            -> CfilterFuture
        {
            return futures_.at(blockHash);
        }

        auto Finish(Index index) noexcept -> void
        {
            auto lock = Lock{lock_};
            promises_.erase(index);
        }
        auto Flush() noexcept -> void
        {
            auto lock = Lock{lock_};
            promises_.clear();
            futures_.clear();
        }
        auto InitBlock(const Work& work) noexcept -> void
        {
            auto lock = Lock{lock_};
            auto& [cfilter, blockhash] = promises_[work.index_];

            for (const auto& data : work.base_->asData().Blocks()) {
                const auto height = data.Height();
                hashes_[height] = blockhash[height].get_future();
            }
        }
        auto InitCfheader(const Work& work) noexcept -> void
        {
            auto lock = Lock{lock_};
            auto& [cfilter, blockhash] = promises_[work.index_];

            for (const auto& block : work.block_hashes_) {
                const auto hash = block->Bytes();
                futures_[hash] = cfilter[hash].get_future();
            }
        }
        auto Set(
            Index index,
            block::Height height,
            const block::Hash& hash) noexcept -> void
        {
            auto& promise = [&]() -> auto&
            {
                auto lock = Lock{lock_};

                return promises_.at(index).second.at(height);
            }
            ();
            promise.set_value(hash);
        }
        auto Set(
            Index index,
            ReadView block,
            const filter::Header& header) noexcept -> void
        {
            auto& promise = [&]() -> auto&
            {
                auto lock = Lock{lock_};

                return promises_.at(index).first.at(block);
            }
            ();
            promise.set_value(header);
        }

        SyncCache() noexcept
            : lock_()
            , hashes_()
            , promises_()
            , futures_()
        {
        }

    private:
        using HashMap = std::map<block::Height, BlockFuture>;
        using CfilterPromise = std::promise<filter::pHeader>;
        using BlockPromise = std::promise<block::pHash>;
        using CfilterPromiseMap = std::map<ReadView, CfilterPromise>;
        using BlockPromiseMap = std::map<block::Height, BlockPromise>;
        using PromiseMap =
            std::map<Index, std::pair<CfilterPromiseMap, BlockPromiseMap>>;
        using FutureMap = std::map<ReadView, CfilterFuture>;

        mutable std::mutex lock_;
        HashMap hashes_;
        PromiseMap promises_;
        FutureMap futures_;

        SyncCache(const SyncCache&) = delete;
        SyncCache(SyncCache&&) = delete;
        auto operator=(const SyncCache&) -> SyncCache& = delete;
        auto operator=(SyncCache&&) -> SyncCache& = delete;
    };

    const api::internal::Core& api_;
    const zmq::Context& zmq_;
    const node::internal::Manager& parent_;
    node::internal::HeaderOracle& header_;
    const node::internal::FilterOracle& filter_;
    mutable std::mutex lock_;
    Index counter_;
    std::map<Index, Work> tasks_;
    SyncCache cache_;
    OTZMQListenCallback stage_1_;
    OTZMQPairSocket socket_;
    OTZMQPipeline stage_2_;
    OTZMQPipeline stage_3_;
    OTZMQPipeline stage_4_;
    OTZMQPipeline stage_5_;
    OTZMQPipeline stage_6_;
    OTZMQPipeline stage_7_;

    auto get_work(Index index) noexcept -> Work&
    {
        auto lock = Lock{lock_};

        return tasks_.at(index);
    }

    auto stage_1(zmq::Message& in) noexcept -> void
    {
        const auto body = in.Body();

        OT_ASSERT(1 < body.size());

        auto* p = reinterpret_cast<Base*>(body.at(1).as<std::uintptr_t>());

        OT_ASSERT(nullptr != p);

        auto index = [&] {
            auto lock = Lock{lock_};
            const auto out = ++counter_;
            auto [it, added] = tasks_.try_emplace(out, p, out);

            OT_ASSERT(added);

            lock.unlock();
            const auto& data = it->second;
            cache_.InitBlock(data);

            return out;
        }();
        stage_2_->Push(zmq_.Message(index));
    }
    auto stage_2(zmq::Message& in) noexcept -> void
    {
        auto& work = get_work(in.at(0).as<Index>());

        for (const auto& data : work.base_->asData().Blocks()) {
            work.block_headers_.emplace_back(
                factory::BitcoinBlockHeader(api_, data.Chain(), data.Header()));
        }

        stage_3_->Push(in);
    }
    auto stage_3(zmq::Message& in) noexcept -> void
    {
        auto& work = get_work(in.at(0).as<Index>());
        const auto& data = work.base_->asData().Blocks();
        const auto& headers = work.block_headers_;
        auto d = data.begin();

        for (auto h{headers.begin()}; h < headers.end(); ++d, ++h) {
            const auto& header = *h;
            const auto& hash = work.block_hashes_.emplace_back(header->Hash());
            cache_.Set(work.index_, d->Height(), hash);
        }

        parent_.UpdateRemoteHeight(work.Tip());
        cache_.InitCfheader(work);
        stage_4_->Push(in);
    }
    auto stage_4(zmq::Message& in) noexcept -> void
    {
        auto& work = get_work(in.at(0).as<Index>());
        const auto& data = work.base_->asData().Blocks();
        const auto& hashes = work.block_hashes_;
        const auto params =
            blockchain::internal::GetFilterParams(data.front().FilterType());
        auto b{hashes.begin()};

        for (auto s{data.begin()}; s < data.end(); ++b, ++s) {
            const auto& sync = *s;
            const auto& hash = *b;
            const auto bytes = hash->Bytes();
            work.filters_.emplace_back(
                bytes,
                factory::GCS(
                    api_,
                    params.first,
                    params.second,
                    blockchain::internal::BlockHashToFilterKey(bytes),
                    sync.FilterElements(),
                    sync.Filter()));
        }

        stage_5_->Push(in);
    }
    auto stage_5(zmq::Message& in) noexcept -> void
    {
        auto& work = get_work(in.at(0).as<Index>());

        for (auto& [block, gcs] : work.filters_) {
            work.filter_hashes_.emplace_back(gcs->Hash());
        }

        stage_6_->Push(in);
    }
    auto stage_6(zmq::Message& in) noexcept -> void
    {
        auto& work = get_work(in.at(0).as<Index>());
        auto previous = [&] {
            const auto& first = work.base_->asData().Blocks().front();
            const auto height = first.Height();

            if (0 == height) { return block::BlankHash(); }

            const auto target = height - 1;
            const auto blockHash = [&]() -> block::pHash {
                auto pending = cache_.Get(target);

                if (pending.has_value()) { return pending.value().get(); }

                return header_.BestHash(target);
            }();

            try {

                return cache_.Get(blockHash->Bytes()).get();
            } catch (...) {

                return filter_.LoadFilterHeader(first.FilterType(), blockHash);
            }
        }();
        const auto& filters = work.filters_;
        auto b{work.block_hashes_.begin()};
        auto c{work.filter_hashes_.begin()};

        for (auto f{filters.begin()}; f < filters.end(); ++b, ++c, ++f) {
            const auto& [block, gcs] = *f;
            const auto& hash = *b;
            const auto& [bhash, cfheader, cfhash] =
                work.filter_headers_.emplace_back(
                    hash, gcs->Header(previous->Bytes()), (*c)->Bytes());
            cache_.Set(work.index_, block, cfheader);
            previous = cfheader;
        }

        stage_7_->Push(in);
    }
    auto stage_7(zmq::Message& in) noexcept -> void
    {
        auto& work = get_work(in.at(0).as<Index>());
        const auto index = work.index_;
        const auto tip = work.Tip();

        if (header_.AddHeaders(work.block_headers_)) {
            filter_.ProcessSyncData(
                work.base_->asData().Blocks().front().FilterType(),
                tip,
                work.filters_,
                work.filter_headers_);
        }
        cache_.Finish(index);
        Init();
        auto lock = Lock{lock_};
        tasks_.erase(index);
    }
};

SyncPipeline::SyncPipeline(
    const api::internal::Core& api,
    const node::internal::Manager& parent,
    node::internal::HeaderOracle& header,
    const node::internal::FilterOracle& filter,
    const std::string& endpoint) noexcept
    : imp_(std::make_unique<Imp>(api, parent, header, filter, endpoint))
{
}

auto SyncPipeline::Init() const noexcept -> void { imp_->Init(); }

auto SyncPipeline::ProcessThreadPool(
    const network::zeromq::Message& task) noexcept -> void
{
    // FIXME
}

SyncPipeline::~SyncPipeline() = default;
}  // namespace opentxs::blockchain::node::base
