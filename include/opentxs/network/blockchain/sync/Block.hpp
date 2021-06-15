// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/blockchain/BlockchainType.hpp"
// IWYU pragma: no_include "opentxs/blockchain/FilterType.hpp"

#ifndef OPENTXS_NETWORK_BLOCKCHAIN_SYNC_BLOCK_HPP
#define OPENTXS_NETWORK_BLOCKCHAIN_SYNC_BLOCK_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>

#include "opentxs/Bytes.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/Types.hpp"

namespace opentxs
{
namespace proto
{
class BlockchainP2PSync;
}  // namespace proto
}  // namespace opentxs

namespace opentxs
{
namespace network
{
namespace blockchain
{
namespace sync
{
class OPENTXS_EXPORT Block
{
public:
    auto Chain() const noexcept -> opentxs::blockchain::Type;
    auto Filter() const noexcept -> ReadView;
    auto FilterElements() const noexcept -> std::uint32_t;
    auto FilterType() const noexcept -> opentxs::blockchain::filter::Type;
    auto Header() const noexcept -> ReadView;
    auto Height() const noexcept -> opentxs::blockchain::block::Height;
    OPENTXS_NO_EXPORT auto Serialize(
        proto::BlockchainP2PSync& dest) const noexcept -> bool;
    OPENTXS_NO_EXPORT auto Serialize(AllocateOutput dest) const noexcept
        -> bool;

    OPENTXS_NO_EXPORT auto operator=(Block&& rhs) noexcept -> Block&;

    OPENTXS_NO_EXPORT Block(
        const proto::BlockchainP2PSync& serialized) noexcept(false);
    OPENTXS_NO_EXPORT Block(
        opentxs::blockchain::Type chain,
        opentxs::blockchain::block::Height height,
        opentxs::blockchain::filter::Type type,
        std::uint32_t count,
        ReadView header,
        ReadView filter) noexcept(false);
    OPENTXS_NO_EXPORT Block(Block&&) noexcept;

    OPENTXS_NO_EXPORT ~Block();

private:
    struct Imp;

    Imp* imp_;

    Block() noexcept;
    Block(const Block&) = delete;
    auto operator=(const Block&) -> Block& = delete;
};
}  // namespace sync
}  // namespace blockchain
}  // namespace network
}  // namespace opentxs
#endif
