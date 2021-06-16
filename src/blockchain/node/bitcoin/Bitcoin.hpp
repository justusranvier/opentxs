// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <string>

#include "blockchain/node/base/Base.hpp"
#include "opentxs/Bytes.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"

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
}  // namespace api

namespace blockchain
{
namespace block
{
class Header;
}  // namespace block

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
struct Config;
struct Manager;
}  // namespace internal
}  // namespace node
}  // namespace blockchain
}  // namespace opentxs

namespace opentxs::blockchain::node::base
{
class Bitcoin final : public node::implementation::Base
{
public:
    auto instantiate_header(const ReadView payload) const noexcept
        -> std::unique_ptr<block::Header> final;

    Bitcoin(
        const api::internal::Core& api,
        const api::client::internal::Blockchain& crypto,
        const api::network::internal::Blockchain& network,
        const Type type,
        const internal::Config& config,
        const std::string& seednode,
        const std::string& syncEndpoint);
    ~Bitcoin() final;

private:
    using ot_super = node::implementation::Base;

    Bitcoin() = delete;
    Bitcoin(const Bitcoin&) = delete;
    Bitcoin(Bitcoin&&) = delete;
    auto operator=(const Bitcoin&) -> Bitcoin& = delete;
    auto operator=(Bitcoin&&) -> Bitcoin& = delete;
};
}  // namespace opentxs::blockchain::node::base
