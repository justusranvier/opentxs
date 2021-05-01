// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_NETWORK_BLOCKCHAIN_BITMESSAGE_CLIENT_HPP
#define OPENTXS_NETWORK_BLOCKCHAIN_BITMESSAGE_CLIENT_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

namespace opentxs
{
namespace network
{
namespace blockchain
{
namespace bitmessage
{
class OPENTXS_EXPORT Client
{
public:
    struct Imp;

    OPENTXS_NO_EXPORT auto Start() const noexcept -> bool;
    OPENTXS_NO_EXPORT auto Stop() const noexcept -> bool;

    OPENTXS_NO_EXPORT Client(Imp* imp) noexcept;
    OPENTXS_NO_EXPORT Client(Client&&) noexcept;

    OPENTXS_NO_EXPORT ~Client();

private:
    Imp* imp_;

    Client() = delete;
    Client(const Client&) = delete;
    auto operator=(const Client&) -> Client& = delete;
    auto operator=(Client&&) -> Client& = delete;
};
}  // namespace bitmessage
}  // namespace blockchain
}  // namespace network
}  // namespace opentxs
#endif
