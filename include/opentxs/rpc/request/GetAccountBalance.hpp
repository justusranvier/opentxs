// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_RPC_GET_ACCOUNT_BALANCE_HPP
#define OPENTXS_RPC_GET_ACCOUNT_BALANCE_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <set>

#include "opentxs/Types.hpp"
#include "opentxs/rpc/request/Base.hpp"

namespace opentxs
{
namespace proto
{
class RPCCommand;
}  // namespace proto
}  // namespace opentxs

namespace opentxs
{
namespace rpc
{
namespace request
{
class GetAccountBalance final : public Base
{
public:
    OPENTXS_EXPORT static auto DefaultVersion() noexcept -> VersionNumber;

    OPENTXS_EXPORT auto Accounts() const noexcept -> const Identifiers&;

    /// throws std::runtime_error for invalid constructor arguments
    OPENTXS_EXPORT GetAccountBalance(
        SessionIndex session,
        const Identifiers& accounts,
        const AssociateNyms& nyms = {}) noexcept(false);
    GetAccountBalance(const proto::RPCCommand& serialized) noexcept(false);
    OPENTXS_EXPORT GetAccountBalance() noexcept;

    OPENTXS_EXPORT ~GetAccountBalance() final;

private:
    GetAccountBalance(const GetAccountBalance&) = delete;
    GetAccountBalance(GetAccountBalance&&) = delete;
    auto operator=(const GetAccountBalance&) -> GetAccountBalance& = delete;
    auto operator=(GetAccountBalance&&) -> GetAccountBalance& = delete;
};
}  // namespace request
}  // namespace rpc
}  // namespace opentxs
#endif
