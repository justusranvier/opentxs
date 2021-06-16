// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                      // IWYU pragma: associated
#include "1_Internal.hpp"                    // IWYU pragma: associated
#include "internal/api/network/Factory.hpp"  // IWYU pragma: associated

#include "api/network/blockchain/Imp.hpp"
#include "opentxs/api/network/Blockchain.hpp"

namespace opentxs::factory
{
using ReturnType = api::network::BlockchainImp;

auto BlockchainNetworkAPI(
    const api::internal::Core& api,
    const api::Endpoints& endpoints,
    const opentxs::network::zeromq::Context& zmq) noexcept
    -> api::network::Blockchain::Imp*
{
    return std::make_unique<ReturnType>(api, endpoints, zmq).release();
}
}  // namespace opentxs::factory
