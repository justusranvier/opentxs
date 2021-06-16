// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                    // IWYU pragma: associated
#include "1_Internal.hpp"                  // IWYU pragma: associated
#include "api/network/blockchain/Imp.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <chrono>
#include <iterator>
#include <utility>

#include "api/network/blockchain/SyncClient.hpp"
#include "api/network/blockchain/SyncServer.hpp"
#include "blockchain/database/common/Database.hpp"
#include "core/Worker.hpp"
#include "internal/api/Api.hpp"
#include "internal/blockchain/Params.hpp"
#include "internal/blockchain/database/Database.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/ThreadPool.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/blockchain/p2p/Types.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/util/WorkType.hpp"

#define OT_METHOD "opentxs::api::network::BlockchainImp::"

namespace opentxs::api::network
{
BlockchainImp::BlockchainImp(
    const api::internal::Core& api,
    const api::Endpoints& endpoints,
    const opentxs::network::zeromq::Context& zmq) noexcept
    : api_(api)
    , crypto_(nullptr)
    , db_(nullptr)
    , active_peer_updates_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainPeer());

        OT_ASSERT(listen);

        return out;
    }())
    , block_download_queue_([&] {
        auto out = zmq.PublishSocket();
        const auto listen =
            out->Start(endpoints.BlockchainBlockDownloadQueue());

        OT_ASSERT(listen);

        return out;
    }())
    , chain_state_publisher_([&] {
        auto out = zmq.PublishSocket();
        auto rc = out->Start(endpoints.BlockchainStateChange());

        OT_ASSERT(rc);

        return out;
    }())
    , connected_peer_updates_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainPeerConnection());

        OT_ASSERT(listen);

        return out;
    }())
    , new_filters_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainNewFilter());

        OT_ASSERT(listen);

        return out;
    }())
    , reorg_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainReorg());

        OT_ASSERT(listen);

        return out;
    }())
    , sync_updates_([&] {
        auto out = zmq.PublishSocket();
        const auto listen = out->Start(endpoints.BlockchainSyncProgress());

        OT_ASSERT(listen);

        return out;
    }())
    , base_config_(nullptr)
    , lock_()
    , config_()
    , networks_()
    , sync_client_()
    , sync_server_()
    , init_promise_()
    , init_(init_promise_.get_future())
    , running_(true)
    , heartbeat_(&BlockchainImp::heartbeat, this)
{
    using Work = api::internal::ThreadPool::Work;
    using Wallet = opentxs::blockchain::node::internal::Wallet;
    using Filters = opentxs::blockchain::node::internal::FilterOracle;
    constexpr auto value = [](auto work) {
        return static_cast<OTZMQWorkType>(work);
    };
    const auto& pool = api_.ThreadPool();
    pool.Register(value(Work::BlockchainWallet), [](const auto& work) {
        Wallet::ProcessThreadPool(work);
    });
    pool.Register(value(Work::CalculateBlockFilters), [](const auto& work) {
        Filters::ProcessThreadPool(work);
    });
}

auto BlockchainImp::AddSyncServer(const std::string& endpoint) const noexcept
    -> bool
{
    init_.get();

    return db_->AddSyncServer(endpoint);
}

auto BlockchainImp::ConnectedSyncServers() const noexcept -> Endpoints
{
    return {};  // TODO
}

auto BlockchainImp::DeleteSyncServer(const std::string& endpoint) const noexcept
    -> bool
{
    init_.get();

    return db_->DeleteSyncServer(endpoint);
}

auto BlockchainImp::Disable(const Chain type) const noexcept -> bool
{
    auto lock = Lock{lock_};

    return disable(lock, type);
}

auto BlockchainImp::disable(const Lock& lock, const Chain type) const noexcept
    -> bool
{
    if (0 == opentxs::blockchain::SupportedChains().count(type)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported chain").Flush();

        return false;
    }

    stop(lock, type);

    if (db_->Disable(type)) { return true; }

    LogOutput(OT_METHOD)(__FUNCTION__)(": Database update failure").Flush();

    return false;
}

auto BlockchainImp::Enable(const Chain type, const std::string& seednode)
    const noexcept -> bool
{
    auto lock = Lock{lock_};

    return enable(lock, type, seednode);
}

auto BlockchainImp::enable(
    const Lock& lock,
    const Chain type,
    const std::string& seednode) const noexcept -> bool
{
    if (0 == opentxs::blockchain::SupportedChains().count(type)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported chain").Flush();

        return false;
    }

    init_.get();

    if (false == db_->Enable(type, seednode)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Database error").Flush();

        return false;
    }

    return start(lock, type, seednode);
}

auto BlockchainImp::EnabledChains() const noexcept -> std::set<Chain>
{
    auto out = std::set<Chain>{};
    init_.get();
    const auto data = [&] {
        auto lock = Lock{lock_};

        return db_->LoadEnabledChains();
    }();
    std::transform(
        data.begin(),
        data.end(),
        std::inserter(out, out.begin()),
        [](const auto value) { return value.first; });

    return out;
}

auto BlockchainImp::GetChain(const Chain type) const noexcept(false)
    -> const opentxs::blockchain::node::Manager&
{
    auto lock = Lock{lock_};

    return *networks_.at(type);
}

auto BlockchainImp::GetSyncServers() const noexcept -> Endpoints
{
    init_.get();

    return db_->GetSyncServers();
}

auto BlockchainImp::heartbeat() const noexcept -> void
{
    init_.get();

    while (running_) {
        auto counter{-1};

        while (running_ && (20 > ++counter)) {
            Sleep(std::chrono::milliseconds{250});
        }

        auto lock = Lock{lock_};

        for (const auto& [key, value] : networks_) {
            if (false == running_) { return; }

            value->Heartbeat();
        }
    }
}

auto BlockchainImp::Hello() const noexcept -> SyncData
{
    auto lock = Lock{lock_};
    auto chains = [&] {
        auto output = Chains{};

        for (const auto& [chain, network] : networks_) {
            output.emplace_back(chain);
        }

        return output;
    }();

    return hello(lock, chains);
}

auto BlockchainImp::hello(const Lock&, const Chains& chains) const noexcept
    -> SyncData
{
    auto output = SyncData{};

    for (const auto chain : chains) {
        const auto& network = networks_.at(chain);
        const auto& filter = network->FilterOracle();
        const auto type = filter.DefaultType();
        output.emplace_back(chain, filter.FilterTip(type));
    }

    return output;
}

auto BlockchainImp::Init(
    const api::client::internal::Blockchain& crypto,
    const api::Legacy& legacy,
    const std::string& dataFolder,
    const ArgList& args) noexcept -> void
{
    crypto_ = &crypto;
    db_ = std::make_unique<opentxs::blockchain::database::common::Database>(
        api_, crypto, legacy, dataFolder, args);

    OT_ASSERT(db_);

    const_cast<std::unique_ptr<Config>&>(base_config_) = [&] {
        auto out = std::make_unique<Config>();
        auto& output = *out;
        const auto sync = [&] {
            try {
                const auto& arg = args.at(OPENTXS_ARG_BLOCKCHAIN_SYNC);

                if (0 == arg.size()) { return false; }

                output.sync_endpoint_ = *arg.begin();

                return true;
            } catch (...) {

                return false;
            }
        }();

        using Policy = opentxs::blockchain::database::BlockStorage;

        if (Policy::All == db_->BlockPolicy()) {
            output.generate_cfilters_ = true;

            if (sync) {
                output.provide_sync_server_ = true;
                output.disable_wallet_ = true;
            }
        } else if (sync) {
            output.use_sync_server_ = true;

        } else {
            output.download_cfilters_ = true;
        }

        try {
            const auto& arg = args.at("disableblockchainwallet");

            if (0 < arg.size()) { output.disable_wallet_ = true; }
        } catch (...) {
        }

        return out;
    }();
    sync_client_ = [&]() -> std::unique_ptr<blockchain::SyncClient> {
        if (base_config_->use_sync_server_) {
            return std::make_unique<blockchain::SyncClient>(api_);
        }

        return {};
    }();
    sync_server_ = [&]() -> std::unique_ptr<blockchain::SyncServer> {
        if (base_config_->provide_sync_server_) {
            return std::make_unique<blockchain::SyncServer>(api_, *this);
        }

        return {};
    }();
    init_promise_.set_value();

    try {
        const auto& endpoints = args.at(OPENTXS_ARG_BLOCKCHAIN_SYNC);

        for (const auto& endpoint : endpoints) { AddSyncServer(endpoint); }
    } catch (...) {
    }
}

auto BlockchainImp::IsEnabled(
    const opentxs::blockchain::Type chain) const noexcept -> bool
{
    init_.get();
    auto lock = Lock{lock_};

    for (const auto& [enabled, peer] : db_->LoadEnabledChains()) {
        if (chain == enabled) { return true; }
    }

    return false;
}

auto BlockchainImp::publish_chain_state(Chain type, bool state) const -> void
{
    auto work =
        api_.Network().ZeroMQ().TaggedMessage(WorkType::BlockchainStateChange);
    work->AddFrame(type);
    work->AddFrame(state);
    chain_state_publisher_->Send(work);
}

auto BlockchainImp::ReportProgress(
    const Chain chain,
    const opentxs::blockchain::block::Height current,
    const opentxs::blockchain::block::Height target) const noexcept -> void
{
    auto work =
        api_.Network().ZeroMQ().TaggedMessage(WorkType::BlockchainSyncProgress);
    work->AddFrame(chain);
    work->AddFrame(current);
    work->AddFrame(target);
    sync_updates_->Send(work);
}

auto BlockchainImp::RestoreNetworks() const noexcept -> void
{
    init_.get();

    if (sync_client_) { sync_client_->Init(api_.Network().Blockchain()); }

    for (const auto& [chain, peer] : db_->LoadEnabledChains()) {
        Start(chain, peer);
    }
}

auto BlockchainImp::Shutdown() noexcept -> void
{
    if (running_.exchange(false)) {
        if (heartbeat_.joinable()) { heartbeat_.join(); }

        LogVerbose("Shutting down ")(networks_.size())(" blockchain clients")
            .Flush();

        for (auto& [chain, network] : networks_) { network->Shutdown().get(); }

        networks_.clear();
    }

    Imp::Shutdown();
}

auto BlockchainImp::Start(const Chain type, const std::string& seednode)
    const noexcept -> bool
{
    auto lock = Lock{lock_};

    return start(lock, type, seednode);
}

auto BlockchainImp::start(
    const Lock& lock,
    const Chain type,
    const std::string& seednode) const noexcept -> bool
{
    init_.get();

    if (Chain::UnitTest != type) {
        if (0 == opentxs::blockchain::SupportedChains().count(type)) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported chain").Flush();

            return false;
        }
    }

    if (0 != networks_.count(type)) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Chain already running").Flush();

        return true;
    }

    namespace p2p = opentxs::blockchain::p2p;

    switch (
        opentxs::blockchain::params::Data::Chains().at(type).p2p_protocol_) {
        case p2p::Protocol::bitcoin: {
            auto endpoint = std::string{};

            if (sync_server_) {
                sync_server_->Enable(type);
                endpoint = sync_server_->Endpoint(type);
            }

            auto& config = [&]() -> const Config& {
                {
                    auto it = config_.find(type);

                    if (config_.end() != it) { return it->second; }
                }

                auto [it, added] = config_.emplace(type, *base_config_);

                OT_ASSERT(added);

                return it->second;
            }();

            auto [it, added] = networks_.emplace(
                type,
                factory::BlockchainNetworkBitcoin(
                    api_, *crypto_, *this, type, config, seednode, endpoint));
            LogVerbose(OT_METHOD)(__FUNCTION__)(": started chain ")(
                static_cast<std::uint32_t>(type))
                .Flush();
            publish_chain_state(type, true);

            return it->second->Connect();
        }
        case p2p::Protocol::opentxs:
        case p2p::Protocol::ethereum:
        default: {
        }
    }

    return false;
}

auto BlockchainImp::StartSyncServer(
    const std::string& sync,
    const std::string& publicSync,
    const std::string& update,
    const std::string& publicUpdate) const noexcept -> bool
{
    auto lock = Lock{lock_};

    if (sync_server_) {
        return sync_server_->Start(sync, publicSync, update, publicUpdate);
    }

    LogNormal("Blockchain sync server must be enabled at library "
              "initialization time by passing the ")(
        OPENTXS_ARG_BLOCKCHAIN_SYNC)(" option.")
        .Flush();

    return false;
}

auto BlockchainImp::Stop(const Chain type) const noexcept -> bool
{
    auto lock = Lock{lock_};

    return stop(lock, type);
}

auto BlockchainImp::stop(const Lock& lock, const Chain type) const noexcept
    -> bool
{
    auto it = networks_.find(type);

    if (networks_.end() == it) { return true; }

    OT_ASSERT(it->second);

    if (sync_server_) { sync_server_->Disable(type); }

    it->second->Shutdown().get();
    networks_.erase(it);
    LogVerbose(OT_METHOD)(__FUNCTION__)(": stopped chain ")(
        opentxs::print(type))
        .Flush();
    publish_chain_state(type, false);

    return true;
}

auto BlockchainImp::SyncEndpoint() const noexcept -> const std::string&
{
    if (sync_client_) {

        return sync_client_->Endpoint();
    } else {

        return Imp::SyncEndpoint();
    }
}

auto BlockchainImp::UpdatePeer(
    const opentxs::blockchain::Type chain,
    const std::string& address) const noexcept -> void
{
    auto work = MakeWork(api_, WorkType::BlockchainPeerAdded);
    work->AddFrame(chain);
    work->AddFrame(address);
    active_peer_updates_->Send(work);
}

BlockchainImp::~BlockchainImp() = default;
}  // namespace opentxs::api::network
