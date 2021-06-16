// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <string>

#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Types.hpp"

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
namespace internal
{
struct Blockchain;
}  // namespace internal
}  // namespace network

class Core;
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

namespace node
{
namespace internal
{
struct BlockDatabase;
struct BlockOracle;
struct Config;
struct FilterDatabase;
struct FilterOracle;
struct HeaderDatabase;
struct HeaderOracle;
struct Manager;
struct PeerDatabase;
struct PeerManager;
struct Wallet;
struct WalletDatabase;
}  // namespace internal
}  // namespace node

namespace internal
{
struct Database;
}  // namespace internal
}  // namespace blockchain
}  // namespace opentxs

namespace opentxs::factory
{
auto BlockchainDatabase(
    const api::Core& api,
    const api::client::internal::Blockchain& crypto,
    const blockchain::node::internal::Manager& node,
    const blockchain::database::common::Database& db,
    const blockchain::Type type) noexcept
    -> std::unique_ptr<blockchain::internal::Database>;
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
    -> std::unique_ptr<blockchain::node::internal::FilterOracle>;
auto BlockchainNetworkBitcoin(
    const api::internal::Core& api,
    const api::client::internal::Blockchain& crypto,
    const api::network::internal::Blockchain& network,
    const blockchain::Type type,
    const blockchain::node::internal::Config& config,
    const std::string& seednode,
    const std::string& syncEndpoint) noexcept
    -> std::unique_ptr<blockchain::node::internal::Manager>;
OPENTXS_EXPORT auto BlockchainNetworkBitcoin(
    const api::Core& api,
    const api::client::internal::Blockchain& crypto,
    const blockchain::Type type,
    const blockchain::node::internal::Config& config,
    const std::string& seednode,
    const std::string& syncEndpoint) noexcept
    -> std::unique_ptr<blockchain::node::internal::Manager>;
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
    -> std::unique_ptr<blockchain::node::internal::PeerManager>;
auto BlockchainWallet(
    const api::Core& api,
    const api::client::internal::Blockchain& crypto,
    const blockchain::node::internal::Manager& parent,
    const blockchain::node::internal::WalletDatabase& db,
    const blockchain::Type chain,
    const std::string& shutdown)
    -> std::unique_ptr<blockchain::node::internal::Wallet>;
auto BlockOracle(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const blockchain::node::internal::Manager& node,
    const blockchain::node::internal::HeaderOracle& header,
    const blockchain::node::internal::BlockDatabase& db,
    const blockchain::Type chain,
    const std::string& shutdown) noexcept
    -> std::unique_ptr<blockchain::node::internal::BlockOracle>;
auto HeaderOracle(
    const api::Core& api,
    const blockchain::node::internal::HeaderDatabase& database,
    const blockchain::Type type) noexcept
    -> std::unique_ptr<blockchain::node::internal::HeaderOracle>;
}  // namespace opentxs::factory
