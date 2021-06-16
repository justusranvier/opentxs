// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <string>

#include "blockchain/bitcoin/CompactSize.hpp"
#include "internal/blockchain/bitcoin/Bitcoin.hpp"
#include "internal/blockchain/database/Database.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "internal/blockchain/p2p/P2P.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/core/Data.hpp"

namespace opentxs
{
namespace api
{
class Core;
}  // namespace api

namespace blockchain
{
namespace client
{
namespace internal
{
struct BlockOracle;
struct Config;
struct FilterOracle;
struct HeaderOracle;
struct Network;
struct PeerManager;
}  // namespace internal
}  // namespace client

namespace p2p
{
namespace bitcoin
{
class Header;
struct Message;
}  // namespace bitcoin

namespace internal
{
struct Peer;
}  // namespace internal
}  // namespace p2p
}  // namespace blockchain

namespace network
{
namespace zeromq
{
class Frame;
}  // namespace zeromq
}  // namespace network
}  // namespace opentxs

namespace opentxs::factory
{
OPENTXS_EXPORT auto BitcoinP2PHeader(
    const api::Core& api,
    const network::zeromq::Frame& bytes) -> blockchain::p2p::bitcoin::Header*;
OPENTXS_EXPORT auto BitcoinP2PMessage(
    const api::Core& api,
    std::unique_ptr<blockchain::p2p::bitcoin::Header> pHeader,
    const blockchain::p2p::bitcoin::ProtocolVersion version,
    const void* payload = nullptr,
    const std::size_t size = 0) -> blockchain::p2p::bitcoin::Message*;
auto BitcoinP2PPeerLegacy(
    const api::Core& api,
    const blockchain::node::internal::Config& config,
    const blockchain::node::internal::Manager& network,
    const blockchain::node::internal::HeaderOracle& header,
    const blockchain::node::internal::FilterOracle& filter,
    const blockchain::node::internal::BlockOracle& block,
    const blockchain::node::internal::PeerManager& manager,
    const blockchain::database::BlockStorage policy,
    const int id,
    std::unique_ptr<blockchain::p2p::internal::Address> address,
    const std::string& shutdown)
    -> std::unique_ptr<blockchain::p2p::internal::Peer>;
}  // namespace opentxs::factory
