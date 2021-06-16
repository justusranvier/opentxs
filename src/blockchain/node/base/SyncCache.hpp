// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <future>
#include <memory>
#include <optional>

#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/Types.hpp"

namespace opentxs
{
namespace blockchain
{
namespace node
{
namespace base
{
class SyncTask;
}  // namespace base
}  // namespace node
}  // namespace blockchain
}  // namespace opentxs

namespace opentxs::blockchain::node::base
{
class SyncCache
{
public:
    using CfilterFuture = std::shared_future<filter::pHeader>;
    using BlockFuture = std::shared_future<block::pHash>;
    using Index = long long int;

    auto Get(block::Height height) const noexcept -> std::optional<BlockFuture>;
    auto Get(const ReadView blockHash) const noexcept(false) -> CfilterFuture;

    auto Finish(Index index) noexcept -> void;
    auto Flush() noexcept -> void;
    auto InitBlock(const SyncTask& work) noexcept -> void;
    auto InitCfheader(const SyncTask& work) noexcept -> void;
    auto Set(
        Index index,
        block::Height height,
        const block::Hash& hash) noexcept -> void;
    auto Set(Index index, ReadView block, const filter::Header& header) noexcept
        -> void;

    SyncCache() noexcept;

    ~SyncCache();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;

    SyncCache(const SyncCache&) = delete;
    SyncCache(SyncCache&&) = delete;
    auto operator=(const SyncCache&) -> SyncCache& = delete;
    auto operator=(SyncCache&&) -> SyncCache& = delete;
};
}  // namespace opentxs::blockchain::node::base
