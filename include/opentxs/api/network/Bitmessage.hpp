// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/network/blockchain/bitmessage/Client.hpp"

#ifndef OPENTXS_API_NETWORK_BITMESSAGE_HPP
#define OPENTXS_API_NETWORK_BITMESSAGE_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

namespace opentxs
{
namespace network
{
namespace blockchain
{
namespace bitmessage
{
class Client;
}  // namespace bitmessage
}  // namespace blockchain
}  // namespace network
}  // namespace opentxs

namespace opentxs
{
namespace api
{
namespace network
{
class OPENTXS_EXPORT Bitmessage
{
public:
    struct Imp;

    using Client = opentxs::network::blockchain::bitmessage::Client;

    auto Get() const noexcept -> const Client&;
    auto Start() const noexcept -> bool;
    auto Stop() const noexcept -> bool;

    OPENTXS_NO_EXPORT Bitmessage(Imp*) noexcept;
    OPENTXS_NO_EXPORT Bitmessage(Bitmessage&&) noexcept;

    OPENTXS_NO_EXPORT ~Bitmessage();

private:
    Imp* imp_;

    Bitmessage() = delete;
    Bitmessage(const Bitmessage&) = delete;
    Bitmessage& operator=(const Bitmessage&) = delete;
    Bitmessage& operator=(Bitmessage&&) = delete;
};
}  // namespace network
}  // namespace api
}  // namespace opentxs
#endif
