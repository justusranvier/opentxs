// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "internal/network/blockchain/bitmessage/Factory.hpp"
#include "opentxs/api/network/Asio.hpp"
#include "opentxs/api/network/Bitmessage.hpp"
#include "opentxs/network/blockchain/bitmessage/Client.hpp"

namespace opentxs
{
namespace api
{
class Core;
}  // namespace api
}  // namespace opentxs

namespace opentxs::api::network
{
struct Bitmessage::Imp {
    auto Get() const noexcept -> const Client& { return bitmessage_; }
    auto Start() const noexcept -> bool { return bitmessage_.Start(); }
    auto Stop() const noexcept -> bool { return bitmessage_.Stop(); }

    Imp(const api::Core& api) noexcept
        : bitmessage_(factory::BitmessageClient(api))
    {
    }

    ~Imp() { bitmessage_.Stop(); }

private:
    const Client bitmessage_;

    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    Imp& operator=(const Imp&) = delete;
    Imp& operator=(Imp&&) = delete;
};
}  // namespace opentxs::api::network
