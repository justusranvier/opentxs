// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include <iosfwd>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/core/Data.hpp"

namespace opentxs
{
namespace api
{
class Core;
}  // namespace api

namespace blockchain
{
namespace block
{
namespace bitcoin
{
class Header;
}  // namespace bitcoin

class Header;
}  // namespace block

namespace node
{
class UpdateTransaction;
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace blockchain
{
namespace sync
{
class Data;
}  // namespace sync
}  // namespace blockchain

namespace zeromq
{
class Message;
}  // namespace zeromq
}  // namespace network

class Factory;
}  // namespace opentxs

namespace opentxs::blockchain::node::implementation
{
class HeaderOracle final : virtual public node::internal::HeaderOracle
{
public:
    auto Ancestors(
        const block::Position& start,
        const block::Position& target,
        const std::size_t limit) const noexcept(false) -> Positions final;
    auto BestChain() const noexcept -> block::Position final;
    auto BestChain(const block::Position& tip, const std::size_t limit) const
        noexcept(false) -> Positions final;
    auto BestHash(const block::Height height) const noexcept
        -> block::pHash final;
    auto BestHashes(const block::Height start, const std::size_t limit = 0)
        const noexcept -> Hashes final;
    auto BestHashes(
        const block::Height start,
        const block::Hash& stop,
        const std::size_t limit = 0) const noexcept -> Hashes final;
    auto BestHashes(
        const Hashes& previous,
        const block::Hash& stop,
        const std::size_t limit) const noexcept -> Hashes final;
    auto CalculateReorg(const block::Position tip) const noexcept(false)
        -> Positions final;
    auto CommonParent(const block::Position& position) const noexcept
        -> std::pair<block::Position, block::Position> final;
    auto GetCheckpoint() const noexcept -> block::Position final;
    auto GetDefaultCheckpoint() const noexcept -> CheckpointData final;
    auto IsInBestChain(const block::Hash& hash) const noexcept -> bool final;
    auto IsInBestChain(const block::Position& position) const noexcept
        -> bool final;
    auto LoadBitcoinHeader(const block::Hash& hash) const noexcept
        -> std::unique_ptr<block::bitcoin::Header> final;
    auto LoadHeader(const block::Hash& hash) const noexcept
        -> std::unique_ptr<block::Header> final;
    auto RecentHashes() const noexcept -> Hashes final
    {
        return database_.RecentHashes();
    }
    auto Siblings() const noexcept -> std::set<block::pHash> final;

    auto AddCheckpoint(
        const block::Height position,
        const block::Hash& requiredHash) noexcept -> bool final;
    auto AddHeader(std::unique_ptr<block::Header> header) noexcept
        -> bool final;
    auto AddHeaders(std::vector<std::unique_ptr<block::Header>>&) noexcept
        -> bool final;
    auto DeleteCheckpoint() noexcept -> bool final;
    auto Init() noexcept -> void final;

    HeaderOracle(
        const api::Core& api,
        const internal::HeaderDatabase& database,
        const blockchain::Type type) noexcept;

    ~HeaderOracle() final = default;

private:
    struct Candidate {
        bool blacklisted_{false};
        std::deque<block::Position> chain_{};
    };

    using Candidates = std::vector<Candidate>;

    const api::Core& api_;
    const internal::HeaderDatabase& database_;
    const blockchain::Type chain_;
    mutable std::mutex lock_;

    static auto evaluate_candidate(
        const block::Header& current,
        const block::Header& candidate) noexcept -> bool;

    auto best_chain(const Lock& lock) const noexcept -> block::Position;
    auto best_chain(
        const Lock& lock,
        const block::Position& tip,
        const std::size_t limit) const noexcept -> Positions;
    auto best_hashes(
        const Lock& lock,
        const block::Height start,
        const block::Hash& stop,
        const std::size_t limit) const noexcept -> Hashes;
    auto common_parent(const Lock& lock, const block::Position& position)
        const noexcept -> std::pair<block::Position, block::Position>;
    auto is_in_best_chain(const Lock& lock, const block::Hash& hash)
        const noexcept -> std::pair<bool, block::Height>;
    auto is_in_best_chain(const Lock& lock, const block::Position& position)
        const noexcept -> bool;
    auto is_in_best_chain(
        const Lock& lock,
        const block::Height height,
        const block::Hash& hash) const noexcept -> bool;

    auto add_header(
        const Lock& lock,
        UpdateTransaction& update,
        std::unique_ptr<block::Header> header) noexcept -> bool;
    auto apply_checkpoint(
        const Lock& lock,
        const block::Height height,
        UpdateTransaction& update) noexcept -> bool;
    auto choose_candidate(
        const block::Header& current,
        const Candidates& candidates,
        UpdateTransaction& update) noexcept(false) -> std::pair<bool, bool>;
    void connect_children(
        const Lock& lock,
        block::Header& parentHeader,
        Candidates& candidates,
        Candidate& candidate,
        UpdateTransaction& update);
    // Returns true if the child is checkpoint blacklisted
    auto connect_to_parent(
        const Lock& lock,
        const UpdateTransaction& update,
        const block::Header& parent,
        block::Header& child) noexcept -> bool;
    auto initialize_candidate(
        const Lock& lock,
        const block::Header& best,
        const block::Header& parent,
        UpdateTransaction& update,
        Candidates& candidates,
        block::Header& child,
        const block::Hash& stopHash = Data::Factory()) noexcept(false)
        -> Candidate&;
    auto is_disconnected(
        const block::Hash& parent,
        UpdateTransaction& update) noexcept -> const block::Header*;
    void stage_candidate(
        const Lock& lock,
        const block::Header& best,
        Candidates& candidates,
        UpdateTransaction& update,
        block::Header& child) noexcept(false);

    HeaderOracle() = delete;
    HeaderOracle(const HeaderOracle&) = delete;
    HeaderOracle(HeaderOracle&&) = delete;
    auto operator=(const HeaderOracle&) -> HeaderOracle& = delete;
    auto operator=(HeaderOracle&&) -> HeaderOracle& = delete;
};
}  // namespace opentxs::blockchain::node::implementation
