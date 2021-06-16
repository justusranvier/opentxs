// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                     // IWYU pragma: associated
#include "1_Internal.hpp"                   // IWYU pragma: associated
#include "blockchain/node/BlockOracle.hpp"  // IWYU pragma: associated

#include <memory>
#include <vector>

#include "blockchain/node/blockoracle/BlockDownloader.hpp"
#include "core/Worker.hpp"
#include "internal/api/network/Network.hpp"
#include "internal/blockchain/database/Database.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"

#define OT_METHOD "opentxs::blockchain::node::implementation::BlockOracle::"

namespace opentxs::factory
{
auto BlockOracle(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::internal::HeaderOracle& header,
    const blockchain::node::internal::BlockDatabase& db,
    const blockchain::Type chain,
    const std::string& shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::BlockOracle>
{
    using ReturnType = blockchain::node::implementation::BlockOracle;

    return std::make_unique<ReturnType>(
        api, network, node, header, db, chain, shutdown);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::implementation
{
BlockOracle::BlockOracle(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const internal::Manager& node,
    const internal::HeaderOracle& header,
    const internal::BlockDatabase& db,
    const blockchain::Type chain,
    const std::string& shutdown) noexcept
    : Worker(api, std::chrono::milliseconds{500})
    , node_(node)
    , db_(db)
    , lock_()
    , cache_(api, node, db, network.BlockQueueUpdate(), chain)
    , block_downloader_([&]() -> std::unique_ptr<BlockDownloader> {
        using Policy = database::BlockStorage;

        if (Policy::All != db.BlockPolicy()) { return nullptr; }

        return std::make_unique<BlockDownloader>(
            api_, db, header, node_, chain, shutdown);
    }())
    , validator_(get_validator(chain, header))
{
    OT_ASSERT(validator_);

    init_executor({shutdown});
}

auto BlockOracle::GetBlockJob() const noexcept -> BlockJob
{
    auto lock = Lock{lock_};

    if (block_downloader_) {

        return block_downloader_->NextBatch();
    } else {

        return {};
    }
}

auto BlockOracle::Heartbeat() const noexcept -> void
{
    trigger();
    auto lock = Lock{lock_};

    if (block_downloader_) { block_downloader_->Heartbeat(); }
}

auto BlockOracle::Init() noexcept -> void
{
    auto lock = Lock{lock_};

    if (block_downloader_) { block_downloader_->Start(); }
}

auto BlockOracle::LoadBitcoin(const block::Hash& block) const noexcept
    -> BitcoinBlockFuture
{
    auto output = cache_.Request(block);
    trigger();

    return output;
}

auto BlockOracle::LoadBitcoin(const BlockHashes& hashes) const noexcept
    -> BitcoinBlockFutures
{
    auto output = cache_.Request(hashes);
    trigger();

    OT_ASSERT(hashes.size() == output.size());

    return output;
}

auto BlockOracle::pipeline(const zmq::Message& in) noexcept -> void
{
    if (false == running_.get()) { return; }

    const auto body = in.Body();

    if (1 > body.size()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid message").Flush();

        OT_FAIL;
    }

    const auto task = [&] {
        try {

            return body.at(0).as<Task>();
        } catch (...) {

            OT_FAIL;
        }
    }();

    switch (task) {
        case Task::ProcessBlock: {
            if (2 > body.size()) {
                LogOutput(OT_METHOD)(__FUNCTION__)(": No block").Flush();

                OT_FAIL;
            }

            cache_.ReceiveBlock(body.at(1));
            [[fallthrough]];
        }
        case Task::StateMachine: {
            do_work();
        } break;
        case Task::Shutdown: {
            shutdown(shutdown_promise_);
        } break;
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unhandled type").Flush();

            OT_FAIL;
        }
    }
}

auto BlockOracle::shutdown(std::promise<void>& promise) noexcept -> void
{
    {
        auto lock = Lock{lock_};

        if (block_downloader_) { block_downloader_.reset(); }
    }

    if (running_->Off()) {
        cache_.Shutdown();

        try {
            promise.set_value();
        } catch (...) {
        }
    }
}

auto BlockOracle::state_machine() noexcept -> bool
{
    if (false == running_.get()) { return false; }

    return cache_.StateMachine();
}

auto BlockOracle::SubmitBlock(const ReadView in) const noexcept -> void
{
    auto work = MakeWork(Task::ProcessBlock);
    work->AddFrame(in.data(), in.size());
    pipeline_->Push(work);
}

BlockOracle::~BlockOracle() { Shutdown().get(); }
}  // namespace opentxs::blockchain::node::implementation
