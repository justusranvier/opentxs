// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                        // IWYU pragma: associated
#include "1_Internal.hpp"                      // IWYU pragma: associated
#include "opentxs/api/network/Bitmessage.hpp"  // IWYU pragma: associated

#include <memory>

#include "api/network/Bitmessage.hpp"
#include "internal/api/network/Factory.hpp"

namespace opentxs::factory
{
using ReturnType = api::network::Bitmessage;

auto BitmessageAPI(const api::Core& api) noexcept -> ReturnType
{
    return {std::make_unique<ReturnType::Imp>(api).release()};
}
}  // namespace opentxs::factory

namespace opentxs::api::network
{
Bitmessage::Bitmessage(Imp* imp) noexcept
    : imp_(imp)
{
}

auto Bitmessage::Get() const noexcept -> const Client& { return imp_->Get(); }

auto Bitmessage::Start() const noexcept -> bool { return imp_->Start(); }

auto Bitmessage::Stop() const noexcept -> bool { return imp_->Stop(); }

Bitmessage::~Bitmessage() { std::unique_ptr<Imp>{imp_}.reset(); }
}  // namespace opentxs::api::network
