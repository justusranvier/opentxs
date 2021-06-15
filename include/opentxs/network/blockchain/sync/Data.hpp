// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_NETWORK_BLOCKCHAIN_SYNC_DATA_HPP
#define OPENTXS_NETWORK_BLOCKCHAIN_SYNC_DATA_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <vector>

#include "opentxs/Bytes.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/network/blockchain/sync/Base.hpp"
#include "opentxs/network/blockchain/sync/Block.hpp"
#include "opentxs/network/blockchain/sync/State.hpp"
#include "opentxs/util/WorkType.hpp"

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
namespace sync
{
class State;
}  // namespace sync
}  // namespace blockchain
}  // namespace network
}  // namespace opentxs

namespace opentxs
{
namespace network
{
namespace blockchain
{
namespace sync
{
class OPENTXS_EXPORT Data final : public Base
{
public:
    using SyncData = std::vector<Block>;
    using Position = opentxs::blockchain::block::Position;

    auto Blocks() const noexcept -> const SyncData&;
    auto LastPosition(const api::Core& api) const noexcept -> Position;
    auto PreviousCfheader() const noexcept -> ReadView;
    auto State() const noexcept -> const sync::State&;

    OPENTXS_NO_EXPORT auto Add(ReadView data) noexcept -> bool;
    OPENTXS_NO_EXPORT auto Add(Data&& data) noexcept -> bool;

    OPENTXS_NO_EXPORT Data(
        WorkType type,
        sync::State state,
        SyncData blocks,
        ReadView cfheader) noexcept(false);
    OPENTXS_NO_EXPORT Data() noexcept;

    ~Data() final;

private:
    Data(const Data&) = delete;
    Data(Data&&) = delete;
    auto operator=(const Data&) -> Data& = delete;
    auto operator=(Data&&) -> Data& = delete;
};
}  // namespace sync
}  // namespace blockchain
}  // namespace network
}  // namespace opentxs
#endif
