// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

namespace opentxs
{
namespace api
{
class Core;
}  // namespace api

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

namespace opentxs::factory
{
auto BitmessageClient(const api::Core& api) noexcept
    -> network::blockchain::bitmessage::Client;
}  // namespace opentxs::factory
