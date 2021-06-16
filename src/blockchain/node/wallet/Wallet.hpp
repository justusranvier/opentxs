// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/endian/buffers.hpp>
#include <boost/endian/conversion.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <iosfwd>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "1_Internal.hpp"
#include "blockchain/bitcoin/CompactSize.hpp"
#include "blockchain/node/wallet/Account.hpp"
#include "blockchain/node/wallet/Accounts.hpp"
#include "blockchain/node/wallet/DeterministicStateData.hpp"
#include "blockchain/node/wallet/Proposals.hpp"
#include "blockchain/node/wallet/SubchainStateData.hpp"
#include "core/Worker.hpp"
#include "internal/blockchain/block/bitcoin/Bitcoin.hpp"
#include "internal/blockchain/crypto/Crypto.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Bytes.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/blockchain/FilterType.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/bitcoin/Input.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/blockchain/node/Wallet.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/protobuf/BlockchainTransactionOutput.pb.h"
#include "opentxs/protobuf/BlockchainTransactionProposal.pb.h"
#include "opentxs/protobuf/Enums.pb.h"
#include "opentxs/util/WorkType.hpp"
#include "util/JobCounter.hpp"
#include "util/Work.hpp"

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

namespace identifier
{
class Nym;
}  // namespace identifier

namespace network
{
namespace zeromq
{
class Message;
}  // namespace zeromq
}  // namespace network

namespace proto
{
class BlockchainTransactionProposal;
}  // namespace proto

class Identifier;
}  // namespace opentxs

namespace opentxs::blockchain::node::implementation
{
class Wallet final : virtual public node::internal::Wallet,
                     Worker<Wallet, api::Core>
{
public:
    auto ConstructTransaction(
        const proto::BlockchainTransactionProposal& tx,
        std::promise<SendOutcome>&& promise) const noexcept -> void final;
    auto GetBalance() const noexcept -> Balance final;
    auto GetBalance(const identifier::Nym& owner) const noexcept
        -> Balance final;
    auto GetBalance(const identifier::Nym& owner, const Identifier& node)
        const noexcept -> Balance final;
    auto GetOutputs(TxoState type) const noexcept -> std::vector<UTXO> final;
    auto GetOutputs(const identifier::Nym& owner, TxoState type) const noexcept
        -> std::vector<UTXO> final;
    auto GetOutputs(
        const identifier::Nym& owner,
        const Identifier& node,
        TxoState type) const noexcept -> std::vector<UTXO> final;

    auto Init() noexcept -> void final;
    auto Shutdown() noexcept -> std::shared_future<void> final
    {
        return stop_worker();
    }

    Wallet(
        const api::Core& api,
        const api::client::internal::Blockchain& crypto,
        const node::internal::Manager& parent,
        const node::internal::WalletDatabase& db,
        const Type chain,
        const std::string& shutdown) noexcept;

    ~Wallet() final;

private:
    friend Worker<Wallet, api::Core>;

    enum class Work : OTZMQWorkType {
        key = OT_ZMQ_NEW_BLOCKCHAIN_WALLET_KEY_SIGNAL,
        block = value(WorkType::BlockchainNewHeader),
        filter = OT_ZMQ_NEW_FILTER_SIGNAL,
        nym = value(WorkType::NymCreated),
        reorg = value(WorkType::BlockchainReorg),
        statemachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
        shutdown = value(WorkType::Shutdown),
    };

    using DBUTXOs = std::vector<node::internal::WalletDatabase::UTXO>;

    const node::internal::Manager& parent_;
    const node::internal::WalletDatabase& db_;
    const api::client::internal::Blockchain& crypto_;
    const Type chain_;
    const SimpleCallback task_finished_;
    std::atomic_bool enabled_;
    OTZMQPushSocket thread_pool;
    wallet::Accounts accounts_;
    wallet::Proposals proposals_;

    auto convert(const DBUTXOs& in) const noexcept -> std::vector<UTXO>;
    auto pipeline(const zmq::Message& in) noexcept -> void;
    auto process_reorg(const zmq::Message& in) noexcept -> void;
    auto shutdown(std::promise<void>& promise) noexcept -> void;
    auto state_machine() noexcept -> bool;

    Wallet() = delete;
    Wallet(const Wallet&) = delete;
    Wallet(Wallet&&) = delete;
    auto operator=(const Wallet&) -> Wallet& = delete;
    auto operator=(Wallet&&) -> Wallet& = delete;
};
}  // namespace opentxs::blockchain::node::implementation
