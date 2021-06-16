// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                     // IWYU pragma: associated
#include "1_Internal.hpp"                   // IWYU pragma: associated
#include "blockchain/node/PeerManager.hpp"  // IWYU pragma: associated

#include <chrono>
#include <memory>
#include <optional>
#include <string_view>

#include "core/Worker.hpp"
#include "internal/api/network/Network.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "internal/blockchain/p2p/P2P.hpp"  // IWYU pragma: keep
#include "opentxs/Bytes.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/block/Block.hpp"
#include "opentxs/blockchain/block/bitcoin/Transaction.hpp"
#include "opentxs/blockchain/p2p/Address.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"

#define OT_METHOD "opentxs::blockchain::node::implementation::PeerManager::"

namespace opentxs::factory
{
auto BlockchainPeerManager(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::internal::HeaderOracle& headers,
    const blockchain::node::internal::FilterOracle& filter,
    const blockchain::node::internal::BlockOracle& block,
    const blockchain::node::internal::PeerDatabase& database,
    const blockchain::Type type,
    const blockchain::database::BlockStorage policy,
    const std::string& seednode,
    const std::string& shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::PeerManager>
{
    using ReturnType = blockchain::node::implementation::PeerManager;

    return std::make_unique<ReturnType>(
        api,
        network,
        config,
        node,
        headers,
        filter,
        block,
        database,
        type,
        policy,
        seednode,
        shutdown);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::implementation
{
PeerManager::PeerManager(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const internal::Config& config,
    const internal::Manager& node,
    const internal::HeaderOracle& headers,
    const internal::FilterOracle& filter,
    const internal::BlockOracle& block,
    const internal::PeerDatabase& database,
    const Type chain,
    const database::BlockStorage policy,
    const std::string& seednode,
    const std::string& shutdown) noexcept
    : internal::PeerManager()
    , Worker(api, std::chrono::milliseconds(100))
    , network_(network)
    , node_(node)
    , database_(database)
    , chain_(chain)
    , jobs_(api)
    , peers_(
          api,
          network,
          config,
          node,
          headers,
          filter,
          block,
          database_,
          *this,
          policy,
          running_,
          shutdown,
          chain,
          seednode,
          peer_target(chain, policy))
    , verified_lock_()
    , verified_peers_()
    , init_promise_()
    , init_(init_promise_.get_future())
{
    init_executor({shutdown});
}

auto PeerManager::AddIncomingPeer(const int id, std::uintptr_t endpoint)
    const noexcept -> void
{
    auto work = MakeWork(Work::IncomingPeer);
    work->AddFrame(id);
    work->AddFrame(endpoint);
    pipeline_->Push(work);
}

auto PeerManager::AddPeer(const p2p::Address& address) const noexcept -> bool
{
    if (false == running_.get()) { return false; }

    auto address_p = std::make_unique<OTBlockchainAddress>(address);
    auto promise = std::make_unique<std::promise<bool>>();
    auto future = promise->get_future();
    auto work = MakeWork(Work::AddPeer);
    work->AddFrame(reinterpret_cast<std::uintptr_t>(address_p.release()));
    work->AddFrame(reinterpret_cast<std::uintptr_t>(promise.release()));
    pipeline_->Push(work);

    while (running_.get()) {
        if (std::future_status::ready ==
            future.wait_for(std::chrono::seconds(5))) {

            return future.get();
        }
    }

    return false;
}

auto PeerManager::BroadcastBlock(const block::Block& block) const noexcept
    -> bool
{
    if (false == running_.get()) { return false; }

    if (0 == peers_.Count()) { return false; }

    const auto& id = block.ID();
    auto work = jobs_.Work(Task::BroadcastBlock);
    work->AddFrame(id);
    jobs_.Dispatch(work);

    return true;
}

auto PeerManager::BroadcastTransaction(
    const block::bitcoin::Transaction& tx) const noexcept -> bool
{
    if (false == running_.get()) { return false; }

    if (0 == peers_.Count()) { return false; }

    auto bytes = Space{};

    if (false == tx.Serialize(writer(bytes)).has_value()) { return false; }

    const auto view = reader(bytes);
    auto work = jobs_.Work(Task::BroadcastTransaction);
    work->AddFrame(view.data(), view.size());
    jobs_.Dispatch(work);

    return true;
}

auto PeerManager::Connect() noexcept -> bool
{
    if (false == running_.get()) { return false; }

    trigger();

    return true;
}

auto PeerManager::Disconnect(const int id) const noexcept -> void
{
    auto work = MakeWork(Work::Disconnect);
    work->AddFrame(id);
    pipeline_->Push(work);
}

auto PeerManager::GetVerifiedPeerCount() const noexcept -> std::size_t
{
    auto lock = Lock{verified_lock_};

    return verified_peers_.size();
}

auto PeerManager::init() noexcept -> void
{
    init_promise_.set_value();
    trigger();
}

auto PeerManager::JobReady(const Task type) const noexcept -> void
{
    switch (type) {
        case Task::JobAvailableCfheaders: {
            jobs_.Dispatch(jobs_.Work(Task::JobAvailableCfheaders));
        } break;
        case Task::JobAvailableCfilters: {
            jobs_.Dispatch(jobs_.Work(Task::JobAvailableCfilters));
        } break;
        case Task::JobAvailableBlock: {
            jobs_.Dispatch(jobs_.Work(Task::JobAvailableBlock));
        } break;
        default: {
        }
    }
}

auto PeerManager::Listen(const p2p::Address& address) const noexcept -> bool
{
    if (false == running_.get()) { return false; }

    auto address_p = std::make_unique<OTBlockchainAddress>(address);
    auto promise = std::make_unique<std::promise<bool>>();
    auto future = promise->get_future();
    auto work = MakeWork(Work::AddListener);
    work->AddFrame(reinterpret_cast<std::uintptr_t>(address_p.release()));
    work->AddFrame(reinterpret_cast<std::uintptr_t>(promise.release()));
    pipeline_->Push(work);

    while (running_.get()) {
        if (std::future_status::ready ==
            future.wait_for(std::chrono::seconds(5))) {

            return future.get();
        } else {

            return false;
        }
    }

    return false;
}

auto PeerManager::peer_target(
    const Type chain,
    const database::BlockStorage policy) noexcept -> std::size_t
{
    if (Type::UnitTest == chain) { return 0; }

    switch (policy) {
        case database::BlockStorage::All: {

            return 6;
        }
        case database::BlockStorage::Cache: {

            return 4;
        }
        case database::BlockStorage::None:
        default: {

            return 2;
        }
    }
}

auto PeerManager::pipeline(zmq::Message& message) noexcept -> void
{
    if (false == running_.get()) { return; }

    const auto body = message.Body();

    OT_ASSERT(0 < body.size());

    const auto work = [&] {
        try {

            return body.at(0).as<Work>();
        } catch (...) {

            OT_FAIL;
        }
    }();

    switch (work) {
        case Work::Disconnect: {
            OT_ASSERT(1 < body.size());

            const auto id = body.at(1).as<int>();

            {
                auto lock = Lock{verified_lock_};
                verified_peers_.erase(id);
            }

            peers_.Disconnect(id);
            network_.UpdatePeer(chain_, "");
            do_work();
        } break;
        case Work::AddPeer: {
            OT_ASSERT(2 < body.size());

            using Promise = std::promise<bool>;

            auto address_p = std::unique_ptr<OTBlockchainAddress>{
                reinterpret_cast<OTBlockchainAddress*>(
                    body.at(1).as<std::uintptr_t>())};
            auto promise_p = std::unique_ptr<Promise>{
                reinterpret_cast<Promise*>(body.at(2).as<std::uintptr_t>())};

            OT_ASSERT(address_p);
            OT_ASSERT(promise_p);

            const auto& address = address_p->get();
            auto& promise = *promise_p;

            peers_.AddPeer(address, promise);
            do_work();
        } break;
        case Work::AddListener: {
            OT_ASSERT(2 < body.size());

            using Promise = std::promise<bool>;

            auto address_p = std::unique_ptr<OTBlockchainAddress>{
                reinterpret_cast<OTBlockchainAddress*>(
                    body.at(1).as<std::uintptr_t>())};
            auto promise_p = std::unique_ptr<Promise>{
                reinterpret_cast<Promise*>(body.at(2).as<std::uintptr_t>())};

            OT_ASSERT(address_p);
            OT_ASSERT(promise_p);

            const auto& address = address_p->get();
            auto& promise = *promise_p;

            peers_.AddListener(address, promise);
            do_work();
        } break;
        case Work::IncomingPeer: {
            OT_ASSERT(2 < body.size());

            const auto id = body.at(1).as<int>();
            auto endpoint =
                Peers::Endpoint{reinterpret_cast<p2p::internal::Address*>(
                    body.at(2).as<std::uintptr_t>())};

            OT_ASSERT(0 <= id);
            OT_ASSERT(endpoint);

            peers_.AddIncoming(id, std::move(endpoint));
            do_work();
        } break;
        case Work::StateMachine: {
            do_work();
        } break;
        case Work::Shutdown: {
            shutdown(shutdown_promise_);
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto PeerManager::RequestBlock(const block::Hash& block) const noexcept -> bool
{
    if (block.empty()) { return false; }

    return RequestBlocks({block.Bytes()});
}

auto PeerManager::RequestBlocks(
    const std::vector<ReadView>& hashes) const noexcept -> bool
{
    if (false == running_.get()) { return false; }

    if (0 == peers_.Count()) { return false; }

    if (0 == hashes.size()) { return false; }

    auto work = jobs_.Work(Task::Getblock);

    for (const auto& block : hashes) {
        work->AddFrame(block.data(), block.size());
    }

    jobs_.Dispatch(work);

    return true;
}

auto PeerManager::RequestHeaders() const noexcept -> bool
{
    if (false == running_.get()) { return false; }

    if (0 == peers_.Count()) { return false; }

    jobs_.Dispatch(Task::Getheaders);

    return true;
}

auto PeerManager::shutdown(std::promise<void>& promise) noexcept -> void
{
    init_.get();

    if (running_->Off()) {
        jobs_.Shutdown();
        peers_.Shutdown();

        try {
            promise.set_value();
        } catch (...) {
        }
    }
}

auto PeerManager::state_machine() noexcept -> bool
{
    LogTrace(OT_METHOD)(__FUNCTION__).Flush();

    if (false == running_.get()) { return false; }

    return peers_.Run();
}

auto PeerManager::VerifyPeer(const int id, const std::string& address)
    const noexcept -> void
{
    {
        auto lock = Lock{verified_lock_};
        verified_peers_.emplace(id);
    }

    network_.UpdatePeer(chain_, address);
}

PeerManager::~PeerManager() { Shutdown().get(); }
}  // namespace opentxs::blockchain::node::implementation
