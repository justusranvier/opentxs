// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/FilterType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Block.hpp"
#include "opentxs/blockchain/crypto/Subaccount.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/crypto/Types.hpp"

namespace opentxs
{
namespace api
{
namespace client
{
namespace internal
{
struct Blockchain;
}  // namespace internal
}  // namespace client

class Core;
}  // namespace api

namespace blockchain
{
namespace block
{
namespace bitcoin
{
class Block;
class Transaction;
}  // namespace bitcoin
}  // namespace block

namespace crypto
{
class Element;
}  // namespace crypto

namespace node
{
namespace internal
{
struct WalletDatabase;
}  // namespace internal
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace zeromq
{
namespace socket
{
class Push;
}  // namespace socket
}  // namespace zeromq
}  // namespace network

class Outstanding;
}  // namespace opentxs

namespace opentxs::blockchain::node::wallet
{
class SubchainStateData
{
public:
    using WalletDatabase = node::internal::WalletDatabase;
    using Subchain = WalletDatabase::Subchain;
    using OutstandingMap =
        std::map<block::pHash, BlockOracle::BitcoinBlockFuture>;
    using ProcessQueue = std::queue<OutstandingMap::iterator>;
    using SubchainIndex = WalletDatabase::pSubchainIndex;

    struct ReorgQueue {
        auto Empty() const noexcept -> bool;

        auto Queue(const block::Position& parent) noexcept -> bool;
        auto Next() noexcept -> block::Position;

    private:
        mutable std::mutex lock_{};
        std::queue<block::Position> parents_{};
    };

    const OTNymID owner_;
    const OTIdentifier id_;
    const Subchain subchain_;
    const filter::Type filter_type_;
    const SubchainIndex index_;
    Outstanding& job_counter_;
    const SimpleCallback& task_finished_;
    std::atomic<bool> running_;
    ReorgQueue reorg_;
    std::optional<Bip32Index> last_indexed_;
    std::optional<block::Position> last_scanned_;
    std::vector<block::pHash> blocks_to_request_;
    OutstandingMap outstanding_blocks_;
    ProcessQueue process_block_queue_;

    virtual auto index() noexcept -> void = 0;
    virtual auto process() noexcept -> void;
    virtual auto reorg() noexcept -> void;
    virtual auto scan() noexcept -> void;

    auto state_machine() noexcept -> bool;

    virtual ~SubchainStateData();

protected:
    using Task = node::internal::Wallet::Task;
    using Patterns = WalletDatabase::Patterns;
    using UTXOs = std::vector<WalletDatabase::UTXO>;
    using Targets = node::GCS::Targets;
    using Tested = WalletDatabase::MatchingIndices;

    const api::Core& api_;
    const api::client::internal::Blockchain& crypto_;
    const node::internal::Manager& node_;
    const WalletDatabase& db_;
    const std::string name_;
    const block::Position null_position_;

    auto describe() const noexcept -> std::string;
    auto get_account_targets() const noexcept
        -> std::tuple<Patterns, UTXOs, Targets>;
    auto get_block_targets(const block::Hash& id, const UTXOs& utxos)
        const noexcept -> std::pair<Patterns, Targets>;
    auto get_block_targets(const block::Hash& id, Tested& tested) const noexcept
        -> std::tuple<Patterns, UTXOs, Targets, Patterns>;
    virtual auto type() const noexcept -> std::stringstream = 0;
    auto set_key_data(block::bitcoin::Transaction& tx) const noexcept -> void;

    auto index_element(
        const filter::Type type,
        const blockchain::crypto::Element& input,
        const Bip32Index index,
        WalletDatabase::ElementMap& output) noexcept -> void;
    // NOTE call from all and only final constructor bodies
    auto init() noexcept -> void;
    auto queue_work(const Task task, const char* log) noexcept -> bool;

    SubchainStateData(
        const api::Core& api,
        const api::client::internal::Blockchain& crypto,
        const node::internal::Manager& node,
        const WalletDatabase& db,
        OTNymID&& owner,
        OTIdentifier&& id,
        const SimpleCallback& taskFinished,
        Outstanding& jobCounter,
        const zmq::socket::Push& threadPool,
        const filter::Type filter,
        const Subchain subchain) noexcept;

private:
    const zmq::socket::Push& thread_pool_;
    block::Position last_reported_;

    auto get_targets(
        const Patterns& elements,
        const std::vector<WalletDatabase::UTXO>& utxos,
        Targets& targets) const noexcept -> void;
    auto get_targets(
        const Patterns& elements,
        const std::vector<WalletDatabase::UTXO>& utxos,
        Targets& targets,
        Patterns& outpoints,
        Tested& tested) const noexcept -> void;

    auto check_blocks() noexcept -> bool;
    virtual auto check_index() noexcept -> bool = 0;
    auto check_process() noexcept -> bool;
    auto check_reorg() noexcept -> bool;
    auto check_scan() noexcept -> bool;
    virtual auto handle_confirmed_matches(
        const block::bitcoin::Block& block,
        const block::Position& position,
        const block::Block::Matches& confirmed) noexcept -> void = 0;
    auto report_scan() noexcept -> void;

    SubchainStateData() = delete;
    SubchainStateData(const SubchainStateData&) = delete;
    SubchainStateData(SubchainStateData&&) = delete;
    SubchainStateData& operator=(const SubchainStateData&) = delete;
    SubchainStateData& operator=(SubchainStateData&&) = delete;
};
}  // namespace opentxs::blockchain::node::wallet
