// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "opentxs/network/blockchain/bitmessage/Client.hpp"  // IWYU pragma: associated

#include <memory>

#include "internal/network/blockchain/bitmessage/Factory.hpp"
#include "network/blockchain/bitmessage/Client.hpp"

// #define OT_METHOD "opentxs::network::blockchain::bitmessage::Client::"

namespace opentxs::factory
{
using ReturnType = network::blockchain::bitmessage::Client;

auto BitmessageClient(const api::Core& api) noexcept -> ReturnType
{
    return {std::make_unique<ReturnType::Imp>(api).release()};
}
}  // namespace opentxs::factory

namespace opentxs::network::blockchain::bitmessage
{
Client::Imp::Imp(const api::Core& api) noexcept
    : peers_(api)
{
}

Client::Imp::~Imp() = default;
}  // namespace opentxs::network::blockchain::bitmessage

namespace opentxs::network::blockchain::bitmessage
{
Client::Client(Imp* imp) noexcept
    : imp_(imp)
{
}

auto Client::Start() const noexcept -> bool { return imp_->Start(); }

auto Client::Stop() const noexcept -> bool { return imp_->Stop(); }

Client::~Client()
{
    if (nullptr != imp_) {
        std::unique_ptr<Imp>{imp_}.reset();
        imp_ = nullptr;
    }
}
}  // namespace opentxs::network::blockchain::bitmessage
