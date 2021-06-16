// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include "internal/blockchain/crypto/Crypto.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/FilterType.hpp"

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

class Core;
}  // namespace api

namespace blockchain
{
namespace crypto
{
namespace internal
{
struct Account;
}  // namespace internal
}  // namespace crypto

namespace node
{
namespace internal
{
struct Manager;
struct WalletDatabase;
}  // namespace internal

namespace wallet
{
class SubchainStateData;
}  // namespace wallet
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace zeromq
{
namespace socket
{
class Push;
}  // namespace socket
}  // namespace zeromq
}  // namespace network

class Outstanding;
}  // namespace opentxs

namespace opentxs::blockchain::node::wallet
{
class Account
{
public:
    using BalanceTree = crypto::internal::Account;

    auto reorg(const block::Position& parent) noexcept -> bool;
    auto shutdown() noexcept -> void;
    auto state_machine() noexcept -> bool;

    Account(
        const api::Core& api,
        const api::client::internal::Blockchain& crypto,
        const BalanceTree& ref,
        const node::internal::Manager& node,
        const node::internal::WalletDatabase& db,
        const network::zeromq::socket::Push& threadPool,
        const filter::Type filter,
        Outstanding&& jobs,
        const SimpleCallback& taskFinished) noexcept;
    Account(Account&&) noexcept;

    ~Account();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;

    Account(const Account&) = delete;
    auto operator=(const Account&) -> Account& = delete;
    auto operator=(Account&&) -> Account& = delete;
};
}  // namespace opentxs::blockchain::node::wallet
