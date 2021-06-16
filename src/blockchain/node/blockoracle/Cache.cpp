// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                     // IWYU pragma: associated
#include "1_Internal.hpp"                   // IWYU pragma: associated
#include "blockchain/node/BlockOracle.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <vector>

#include "internal/blockchain/database/Database.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Bytes.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/block/bitcoin/Block.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/util/WorkType.hpp"

#define OT_METHOD                                                              \
    "opentxs::blockchain::node::implementation::BlockOracle::Cache::"

namespace opentxs::blockchain::node::implementation
{
const std::size_t BlockOracle::Cache::cache_limit_{16};
const std::chrono::seconds BlockOracle::Cache::download_timeout_{15};

BlockOracle::Cache::Cache(
    const api::Core& api,
    const internal::Manager& node,
    const internal::BlockDatabase& db,
    const network::zeromq::socket::Publish& socket,
    const blockchain::Type chain) noexcept
    : api_(api)
    , node_(node)
    , db_(db)
    , cache_size_publisher_(socket)
    , chain_(chain)
    , lock_()
    , pending_()
    , mem_(cache_limit_)
    , running_(true)
{
}

auto BlockOracle::Cache::download(const block::Hash& block) const noexcept
    -> bool
{
    return node_.RequestBlock(block);
}

auto BlockOracle::Cache::DownloadQueue() const noexcept -> std::size_t
{
    auto lock = Lock{lock_};

    return pending_.size();
}

auto BlockOracle::Cache::publish(std::size_t size) const noexcept -> void
{
    auto work = api_.Network().ZeroMQ().TaggedMessage(
        WorkType::BlockchainBlockDownloadQueue);
    work->AddFrame(chain_);
    work->AddFrame(size);
    cache_size_publisher_.Send(work);
}

auto BlockOracle::Cache::ReceiveBlock(const zmq::Frame& in) const noexcept
    -> void
{
    ReceiveBlock(api_.Factory().BitcoinBlock(chain_, in.Bytes()));
}

auto BlockOracle::Cache::ReceiveBlock(BitcoinBlock_p in) const noexcept -> void
{
    if (false == bool(in)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid block").Flush();

        return;
    }

    auto lock = Lock{lock_};
    auto& block = *in;

    if (database::BlockStorage::None != db_.BlockPolicy()) {
        db_.BlockStore(block);
    }

    const auto& id = block.ID();
    auto pending = pending_.find(id);

    if (pending_.end() == pending) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(
            ": Received block not in request list")
            .Flush();

        return;
    }

    auto& [time, promise, future, queued] = pending->second;
    promise.set_value(std::move(in));
    LogVerbose(OT_METHOD)(__FUNCTION__)(": Cached block ")(id.asHex()).Flush();
    mem_.push(std::move(id), std::move(future));
    pending_.erase(pending);
    publish(pending_.size());
}

auto BlockOracle::Cache::Request(const block::Hash& block) const noexcept
    -> BitcoinBlockFuture
{
    const auto output = Request(BlockHashes{block});

    OT_ASSERT(1 == output.size());

    return output.at(0);
}

auto BlockOracle::Cache::Request(const BlockHashes& hashes) const noexcept
    -> BitcoinBlockFutures
{
    auto output = BitcoinBlockFutures{};
    output.reserve(hashes.size());
    auto download = std::map<block::pHash, BitcoinBlockFutures::iterator>{};
    auto lock = Lock{lock_};

    if (false == running_) {
        std::for_each(hashes.begin(), hashes.end(), [&](const auto&) {
            auto promise = Promise{};
            promise.set_value(nullptr);
            output.emplace_back(promise.get_future());
        });

        return output;
    }

    for (const auto& block : hashes) {
        auto found{false};

        if (auto future = mem_.find(block->Bytes()); future.valid()) {
            output.emplace_back(std::move(future));
            found = true;
        }

        if (found) { continue; }

        {
            auto it = pending_.find(block);

            if (pending_.end() != it) {
                const auto& [time, promise, future, queued] = it->second;
                output.emplace_back(future);
                found = true;
            }
        }

        if (found) { continue; }

        if (auto pBlock = db_.BlockLoadBitcoin(block); bool(pBlock)) {
            // TODO this should be checked in the block factory function
            OT_ASSERT(pBlock->ID() == block);

            auto promise = Promise{};
            promise.set_value(std::move(pBlock));
            mem_.push(OTData{block}, promise.get_future());
            output.emplace_back(mem_.find(block->Bytes()));
            found = true;
        }

        if (found) { continue; }

        output.emplace_back();
        auto it = output.begin();
        std::advance(it, output.size() - 1);
        download.emplace(block, it);
    }

    OT_ASSERT(output.size() == hashes.size());

    if (0 < download.size()) {
        auto blockList = std::vector<ReadView>{};
        std::transform(
            std::begin(download),
            std::end(download),
            std::back_inserter(blockList),
            [](const auto& in) -> auto {
                const auto& [key, value] = in;

                return key->Bytes();
            });
        const auto messageSent = node_.RequestBlocks(blockList);

        for (auto& [hash, futureOut] : download) {
            auto& [time, promise, future, queued] = pending_[hash];
            time = Clock::now();
            future = promise.get_future();
            *futureOut = future;
            queued = messageSent;
        }

        publish(pending_.size());
    }

    return output;
}

auto BlockOracle::Cache::Shutdown() noexcept -> void
{
    auto lock = Lock{lock_};

    if (running_) {
        running_ = false;
        mem_.clear();

        for (auto& [hash, item] : pending_) {
            auto& [time, promise, future, queued] = item;
            promise.set_value(nullptr);
        }

        pending_.clear();
        publish(pending_.size());
    }
}

auto BlockOracle::Cache::StateMachine() const noexcept -> bool
{
    auto lock = Lock{lock_};

    if (false == running_) { return false; }

    LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(DisplayString(chain_))(
        " download queue contains ")(pending_.size())(" blocks.")
        .Flush();

    for (auto& [hash, item] : pending_) {
        auto& [time, promise, future, queued] = item;
        const auto now = Clock::now();
        namespace c = std::chrono;
        const auto elapsed = c::duration_cast<c::milliseconds>(now - time);
        const auto timeout = download_timeout_ <= elapsed;

        if (timeout || (false == queued)) {
            LogVerbose(OT_METHOD)(__FUNCTION__)(": Requesting ")(
                DisplayString(chain_))(" block ")(hash->asHex())(" from peers")
                .Flush();
            queued = download(hash);
            time = now;
        } else {
            LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(elapsed.count())(
                " milliseconds elapsed waiting for ")(DisplayString(chain_))(
                " block ")(hash->asHex())
                .Flush();
        }
    }

    return 0 < pending_.size();
}
}  // namespace opentxs::blockchain::node::implementation
