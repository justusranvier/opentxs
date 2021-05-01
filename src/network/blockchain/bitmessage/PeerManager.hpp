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
}  // namespace opentxs

namespace opentxs::network::blockchain::bitmessage
{
class PeerManager
{
public:
    struct Imp;

    PeerManager(const api::Core& api) noexcept;

    ~PeerManager();

private:
    std::unique_ptr<Imp> imp_;

    PeerManager() = delete;
    PeerManager(const PeerManager&) = delete;
    PeerManager(PeerManager&&) = delete;
    auto operator=(const PeerManager& rhs) -> PeerManager& = delete;
    auto operator=(PeerManager&&) noexcept -> PeerManager& = delete;
};
}  // namespace opentxs::network::blockchain::bitmessage
