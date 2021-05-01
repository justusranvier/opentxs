// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <memory>

#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"

namespace opentxs
{
namespace api
{
namespace implementation
{
class Scheduler;
}  // namespace implementation

namespace internal
{
struct Core;
}  // namespace internal

namespace network
{
class Bitmessage;
class Asio;
class Dht;
class Network;
}  // namespace network

class Core;
class Endpoints;
}  // namespace api

namespace network
{
namespace zeromq
{
class Context;
}  // namespace zeromq
}  // namespace network
}  // namespace opentxs

namespace opentxs::factory
{
auto BitmessageAPI(const api::Core& api) noexcept -> api::network::Bitmessage;
auto BlockchainNetworkAPI(
    const api::Core& api,
    const api::Endpoints& endpoints,
    const opentxs::network::zeromq::Context& zmq) noexcept
    -> api::network::Blockchain::Imp*;
auto BlockchainNetworkAPINull() noexcept -> api::network::Blockchain::Imp*;
auto DhtAPI(
    const api::internal::Core& api,
    const network::zeromq::Context& zeromq,
    const api::Endpoints& endpoints,
    const bool defaultEnable,
    std::int64_t& nymPublishInterval,
    std::int64_t& nymRefreshInterval,
    std::int64_t& serverPublishInterval,
    std::int64_t& serverRefreshInterval,
    std::int64_t& unitPublishInterval,
    std::int64_t& unitRefreshInterval) noexcept
    -> std::unique_ptr<api::network::Dht>;
auto NetworkAPI(
    const api::internal::Core& api,
    const api::network::Asio& asio,
    const network::zeromq::Context& zmq,
    const api::Endpoints& endpoints,
    api::network::Blockchain::Imp* blockchain,
    api::implementation::Scheduler& config,
    bool dhtDefault) noexcept -> api::network::Network::Imp*;
}  // namespace opentxs::factory
