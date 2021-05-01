// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/network/blockchain/bitmessage/Client.hpp"  // IWYU pragma: associated

#include "network/blockchain/bitmessage/PeerManager.hpp"

namespace opentxs
{
namespace api
{
class Core;
}  // namespace api
}  // namespace opentxs

namespace opentxs::network::blockchain::bitmessage
{
struct Client::Imp {
    auto Start() const noexcept -> bool { return false; }
    auto Stop() const noexcept -> bool { return false; }

    Imp(const api::Core& api) noexcept;

    ~Imp();

private:
    PeerManager peers_;

    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp& rhs) -> Imp& = delete;
    auto operator=(Imp&&) noexcept -> Imp& = delete;
};
}  // namespace opentxs::network::blockchain::bitmessage
