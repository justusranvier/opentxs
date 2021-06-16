// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "api/network/Blockchain.hpp"
#include "blockchain/database/common/Database.hpp"
#include "internal/api/network/Network.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"

namespace opentxs
{
namespace api
{
namespace client
{
namespace internal
{
struct Blockchain;
}  // namespace internal
}  // namespace client

namespace internal
{
struct Core;
}  // namespace internal

namespace network
{
namespace blockchain
{
struct SyncClient;
struct SyncServer;
}  // namespace blockchain

class Blockchain;
}  // namespace network

class Endpoints;
class Legacy;
}  // namespace api

namespace blockchain
{
namespace database
{
namespace common
{
class Database;
}  // namespace common
}  // namespace database

namespace network
{
namespace zeromq
{
class Context;
}  // namespace zeromq
}  // namespace network

namespace node
{
namespace internal
{
struct Config;
struct Manager;
}  // namespace internal
}  // namespace node
}  // namespace blockchain
}  // namespace opentxs

namespace zmq = opentxs::network::zeromq;

namespace opentxs::api::network
{
struct BlockchainImp final : public Blockchain::Imp {
    auto AddSyncServer(const std::string& endpoint) const noexcept
        -> bool final;
    auto BlockQueueUpdate() const noexcept -> const zmq::socket::Publish& final
    {
        return block_download_queue_;
    }
    auto ConnectedSyncServers() const noexcept -> Endpoints final;
    auto Database() const noexcept
        -> const opentxs::blockchain::database::common::Database& final
    {
        return *db_;
    }
    auto DeleteSyncServer(const std::string& endpoint) const noexcept
        -> bool final;
    auto Disable(const Imp::Chain type) const noexcept -> bool final;
    auto Enable(const Imp::Chain type, const std::string& seednode)
        const noexcept -> bool final;
    auto EnabledChains() const noexcept -> std::set<Imp::Chain> final;
    auto FilterUpdate() const noexcept -> const zmq::socket::Publish& final
    {
        return new_filters_;
    }
    auto Hello() const noexcept -> SyncData final;
    auto IsEnabled(const Chain chain) const noexcept -> bool final;
    auto GetChain(const Imp::Chain type) const noexcept(false)
        -> const opentxs::blockchain::node::Manager& final;
    auto GetSyncServers() const noexcept -> Imp::Endpoints final;
    auto PeerUpdate() const noexcept -> const zmq::socket::Publish& final
    {
        return connected_peer_updates_;
    }
    auto Reorg() const noexcept -> const zmq::socket::Publish& final
    {
        return reorg_;
    }
    auto ReportProgress(
        const Chain chain,
        const opentxs::blockchain::block::Height current,
        const opentxs::blockchain::block::Height target) const noexcept
        -> void final;
    auto RestoreNetworks() const noexcept -> void final;
    auto Start(const Imp::Chain type, const std::string& seednode)
        const noexcept -> bool final;
    auto StartSyncServer(
        const std::string& syncEndpoint,
        const std::string& publicSyncEndpoint,
        const std::string& updateEndpoint,
        const std::string& publicUpdateEndpoint) const noexcept -> bool final;
    auto Stop(const Imp::Chain type) const noexcept -> bool final;
    auto SyncEndpoint() const noexcept -> const std::string& final;
    auto UpdatePeer(
        const opentxs::blockchain::Type chain,
        const std::string& address) const noexcept -> void final;

    auto Init(
        const api::client::internal::Blockchain& crypto,
        const api::Legacy& legacy,
        const std::string& dataFolder,
        const ArgList& args) noexcept -> void final;
    auto Shutdown() noexcept -> void final;

    BlockchainImp(
        const api::internal::Core& api,
        const api::Endpoints& endpoints,
        const opentxs::network::zeromq::Context& zmq) noexcept;

    ~BlockchainImp() final;

private:
    using Config = opentxs::blockchain::node::internal::Config;
    using pNode = std::unique_ptr<opentxs::blockchain::node::internal::Manager>;
    using Chains = std::vector<Chain>;

    const api::internal::Core& api_;
    const api::client::internal::Blockchain* crypto_;
    std::unique_ptr<opentxs::blockchain::database::common::Database> db_;
    OTZMQPublishSocket active_peer_updates_;
    OTZMQPublishSocket block_download_queue_;
    OTZMQPublishSocket chain_state_publisher_;
    OTZMQPublishSocket connected_peer_updates_;
    OTZMQPublishSocket new_filters_;
    OTZMQPublishSocket reorg_;
    OTZMQPublishSocket sync_updates_;
    const std::unique_ptr<Config> base_config_;
    mutable std::mutex lock_;
    mutable std::map<Chain, Config> config_;
    mutable std::map<Chain, pNode> networks_;
    std::unique_ptr<blockchain::SyncClient> sync_client_;
    std::unique_ptr<blockchain::SyncServer> sync_server_;
    std::promise<void> init_promise_;
    std::shared_future<void> init_;
    std::atomic_bool running_;
    std::thread heartbeat_;

    auto disable(const Lock& lock, const Chain type) const noexcept -> bool;
    auto enable(const Lock& lock, const Chain type, const std::string& seednode)
        const noexcept -> bool;
    auto heartbeat() const noexcept -> void;
    auto hello(const Lock&, const Chains& chains) const noexcept -> SyncData;
    auto publish_chain_state(Chain type, bool state) const -> void;
    auto start(const Lock& lock, const Chain type, const std::string& seednode)
        const noexcept -> bool;
    auto stop(const Lock& lock, const Chain type) const noexcept -> bool;

    BlockchainImp() = delete;
    BlockchainImp(const BlockchainImp&) = delete;
    BlockchainImp(BlockchainImp&&) = delete;
    auto operator=(const BlockchainImp&) -> BlockchainImp& = delete;
    auto operator=(BlockchainImp&&) -> BlockchainImp& = delete;
};
}  // namespace opentxs::api::network
