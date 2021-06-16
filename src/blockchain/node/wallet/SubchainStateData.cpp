// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/wallet/SubchainStateData.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <chrono>
#include <future>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

#include "internal/api/Api.hpp"
#include "internal/api/client/Client.hpp"
#include "opentxs/Bytes.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/FilterType.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/block/bitcoin/Block.hpp"
#include "opentxs/blockchain/block/bitcoin/Script.hpp"
#include "opentxs/blockchain/block/bitcoin/Transaction.hpp"
#include "opentxs/blockchain/crypto/Element.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/crypto/key/EllipticCurve.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/protobuf/BlockchainTransactionOutput.pb.h"  // IWYU pragma: keep
#include "opentxs/protobuf/BlockchainWalletKey.pb.h"
#include "util/JobCounter.hpp"
#include "util/ScopeGuard.hpp"

#define OT_METHOD "opentxs::blockchain::node::wallet::SubchainStateData::"

namespace opentxs::blockchain::node::internal
{
auto Wallet::ProcessThreadPool(const zmq::Message& in) noexcept -> void
{
    const auto body = in.Body();

    if (2 > body.size()) {
        LogOutput("opentxs::blockchain::node::internal:Wallet::")(__FUNCTION__)(
            ": Invalid message")
            .Flush();

        OT_FAIL;
    }

    auto* pData = reinterpret_cast<node::wallet::SubchainStateData*>(
        body.at(1).as<std::uintptr_t>());

    OT_ASSERT(nullptr != pData);

    auto& data = *pData;
    auto postcondition = ScopeGuard{[&] {
        data.running_.store(false);
        data.task_finished_();
        --data.job_counter_;
    }};

    const auto task = [&] {
        try {

            return body.at(0).as<Task>();
        } catch (...) {

            OT_FAIL;
        }
    }();

    switch (task) {
        case Task::index: {
            data.index();
        } break;
        case Task::scan: {
            data.scan();
        } break;
        case Task::process: {
            data.process();
        } break;
        case Task::reorg: {
            data.reorg();
        } break;
        default: {
            OT_FAIL;
        }
    }
}
}  // namespace opentxs::blockchain::node::internal

namespace opentxs::blockchain::node::wallet
{
SubchainStateData::SubchainStateData(
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
    const Subchain subchain) noexcept
    : owner_(std::move(owner))
    , id_(std::move(id))
    , subchain_(subchain)
    , filter_type_(filter)
    , index_(db.GetIndex(id_, subchain_, filter_type_))
    , job_counter_(jobCounter)
    , task_finished_(taskFinished)
    , running_(false)
    , reorg_()
    , last_indexed_(db.SubchainLastIndexed(index_))
    , last_scanned_(db.SubchainLastScanned(index_))
    , blocks_to_request_()
    , outstanding_blocks_()
    , process_block_queue_()
    , api_(api)
    , crypto_(crypto)
    , node_(node)
    , db_(db)
    , name_()
    , null_position_(make_blank<block::Position>::value(api_))
    , thread_pool_(threadPool)
    , last_reported_(null_position_)
{
    OT_ASSERT(task_finished_);
    OT_ASSERT(false == owner_->empty());
    OT_ASSERT(false == id_->empty());
}

auto SubchainStateData::ReorgQueue::Empty() const noexcept -> bool
{
    Lock lock(lock_);

    return 0 == parents_.size();
}

auto SubchainStateData::ReorgQueue::Queue(
    const block::Position& parent) noexcept -> bool
{
    Lock lock(lock_);
    parents_.push(parent);

    return true;
}

auto SubchainStateData::ReorgQueue::Next() noexcept -> block::Position
{
    OT_ASSERT(false == Empty());

    Lock lock(lock_);
    auto output{parents_.front()};
    parents_.pop();

    return output;
}

auto SubchainStateData::check_blocks() noexcept -> bool
{
    for (const auto& hash : blocks_to_request_) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" requesting block ")(
            hash->asHex())(" queue position: ")(outstanding_blocks_.size())
            .Flush();

        if (0 == outstanding_blocks_.count(hash)) {
            auto [it, added] = outstanding_blocks_.emplace(
                hash, node_.BlockOracle().LoadBitcoin(hash));

            OT_ASSERT(added);

            process_block_queue_.push(it);
        }
    }

    blocks_to_request_.clear();

    return false;
}

auto SubchainStateData::check_process() noexcept -> bool
{
    if (process_block_queue_.empty()) { return false; }

    const auto& [id, future] = *process_block_queue_.front();

    if (std::future_status::ready ==
        future.wait_for(std::chrono::milliseconds(1))) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(
            " ready to process block")
            .Flush();
        static constexpr auto job{"process"};

        return queue_work(Task::process, job);
    } else {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" waiting for block ")(
            id->asHex())(" to download")
            .Flush();
    }

    return false;
}

auto SubchainStateData::check_reorg() noexcept -> bool
{
    if (reorg_.Empty()) { return false; }

    static constexpr auto job{"reorg"};

    return queue_work(Task::reorg, job);
}

auto SubchainStateData::check_scan() noexcept -> bool
{
    auto needScan{false};

    if (last_scanned_.has_value()) {
        const auto bestFilter =
            node_.FilterOracleInternal().FilterTip(filter_type_);

        if (last_scanned_ == bestFilter) {
            LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(
                " has been scanned to the newest downloaded "
                "filter ")(bestFilter.second->asHex())(" at height ")(
                bestFilter.first)
                .Flush();
        } else {
            const auto [ancestor, best] =
                node_.HeaderOracleInternal().CommonParent(
                    last_scanned_.value());
            last_scanned_ = ancestor;

            if (last_scanned_ == best) {
                LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(
                    " has been scanned to current best block ")(
                    best.second->asHex())(" at height ")(best.first)
                    .Flush();
            } else {
                needScan = true;
                LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(
                    " scanning progress: ")(last_scanned_.value().first)
                    .Flush();
            }
        }
    } else {
        needScan = true;
        LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(
            " scanning progress: ")(0)
            .Flush();
    }

    if (needScan) {
        static constexpr auto job{"scan"};

        return queue_work(Task::scan, job);
    } else {
        report_scan();
    }

    return false;
}

auto SubchainStateData::describe() const noexcept -> std::string
{
    auto out = type();
    out << " account ";
    out << id_->str();
    out << ' ';

    switch (subchain_) {
        case Subchain::Internal: {
            out << "internal";
        } break;
        case Subchain::External: {
            out << "external";
        } break;
        case Subchain::Incoming: {
            out << "incoming";
        } break;
        case Subchain::Outgoing: {
            out << "outgoing";
        } break;
        case Subchain::Notification: {
            out << "notification";
        } break;
        default: {

            OT_FAIL;
        }
    }

    out << " subchain";

    return out.str();
}

auto SubchainStateData::get_account_targets() const noexcept
    -> std::tuple<Patterns, UTXOs, Targets>
{
    auto out = std::tuple<Patterns, UTXOs, Targets>{};
    auto& [elements, utxos, targets] = out;
    elements = db_.GetPatterns(index_);
    utxos = db_.GetUnspentOutputs(id_, subchain_);
    get_targets(elements, utxos, targets);

    return out;
}

auto SubchainStateData::get_block_targets(
    const block::Hash& id,
    const UTXOs& utxos) const noexcept -> std::pair<Patterns, Targets>
{
    auto out = std::pair<Patterns, Targets>{};
    auto& [elements, targets] = out;
    elements = db_.GetUntestedPatterns(index_, id.Bytes());
    get_targets(elements, utxos, targets);

    return out;
}

auto SubchainStateData::get_block_targets(const block::Hash& id, Tested& tested)
    const noexcept -> std::tuple<Patterns, UTXOs, Targets, Patterns>
{
    auto out = std::tuple<Patterns, UTXOs, Targets, Patterns>{};
    auto& [elements, utxos, targets, outpoints] = out;
    elements = db_.GetUntestedPatterns(index_, id.Bytes());
    utxos = db_.GetUnspentOutputs(id_, subchain_);
    get_targets(elements, utxos, targets, outpoints, tested);

    return out;
}

// NOTE: this version is for matching before a block is downloaded
auto SubchainStateData::get_targets(
    const Patterns& elements,
    const std::vector<WalletDatabase::UTXO>& utxos,
    Targets& targets) const noexcept -> void
{
    targets.reserve(elements.size() + utxos.size());

    for (const auto& element : elements) {
        const auto& [id, data] = element;
        targets.emplace_back(reader(data));
    }

    switch (filter_type_) {
        case filter::Type::Basic_BCHVariant:
        case filter::Type::ES: {
            for (const auto& [outpoint, proto] : utxos) {
                targets.emplace_back(outpoint.Bytes());
            }
        } break;
        case filter::Type::Basic_BIP158:
        default: {
        }
    }
}

// NOTE: this version is for matching after a block is downloaded
auto SubchainStateData::get_targets(
    const Patterns& elements,
    const std::vector<WalletDatabase::UTXO>& utxos,
    Targets& targets,
    Patterns& outpoints,
    Tested& tested) const noexcept -> void
{
    targets.reserve(elements.size());
    outpoints.reserve(utxos.size());

    for (const auto& element : elements) {
        const auto& [id, data] = element;
        const auto& [index, subchain] = id;
        targets.emplace_back(reader(data));
        tested.emplace_back(index);
    }

    for (const auto& [outpoint, proto] : utxos) {
        OT_ASSERT(0 < proto.key().size());
        // TODO the assertion below will not always be true in the future but
        // for now it will catch some bugs
        OT_ASSERT(1 == proto.key().size());

        for (const auto& key : proto.key()) {
            auto account = api_.Factory().Identifier(key.subaccount());

            OT_ASSERT(false == account->empty());
            // TODO the assertion below will not always be true in the future
            // but for now it will catch some bugs
            OT_ASSERT(account == id_);

            outpoints.emplace_back(
                WalletDatabase::ElementID{
                    static_cast<Bip32Index>(key.index()),
                    {static_cast<Subchain>(key.subchain()),
                     std::move(account)}},
                space(outpoint.Bytes()));
        }
    }
}

auto SubchainStateData::index_element(
    const filter::Type type,
    const blockchain::crypto::Element& input,
    const Bip32Index index,
    WalletDatabase::ElementMap& output) noexcept -> void
{
    LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" element ")(index)(
        " extracting filter matching patterns")
        .Flush();
    auto& list = output[index];
    auto scripts = std::vector<std::unique_ptr<const block::bitcoin::Script>>{};
    scripts.reserve(2);  // WARNING keep this number up to date if new scripts
                         // are added
    LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" element ")(index)(
        ": using public key ")(
        api_.Factory().Data(input.Key()->PublicKey())->asHex())(
        " for P2PK pattern")
        .Flush();
    const auto& p2pk = scripts.emplace_back(
        api_.Factory().BitcoinScriptP2PK(node_.Chain(), *input.Key()));
    LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" element ")(index)(
        ": using pubkey hash ")(input.PubkeyHash()->asHex())(
        " for P2PKH pattern")
        .Flush();
    const auto& p2pkh = scripts.emplace_back(
        api_.Factory().BitcoinScriptP2PKH(node_.Chain(), *input.Key()));

    OT_ASSERT(p2pk);
    OT_ASSERT(p2pkh);

    switch (type) {
        case filter::Type::ES: {
            OT_ASSERT(p2pk->Pubkey().has_value());
            OT_ASSERT(p2pkh->PubkeyHash().has_value());

            list.emplace_back(space(p2pk->Pubkey().value()));
            list.emplace_back(space(p2pkh->PubkeyHash().value()));
        } break;
        case filter::Type::Basic_BIP158:
        case filter::Type::Basic_BCHVariant:
        default: {
            for (const auto& script : scripts) {
                script->Serialize(writer(list.emplace_back()));
            }
        }
    }
}

auto SubchainStateData::init() noexcept -> void
{
    const_cast<std::string&>(name_) = describe();
}

auto SubchainStateData::process() noexcept -> void
{
    const auto start = Clock::now();
    const auto& filters = node_.FilterOracleInternal();
    auto it = process_block_queue_.front();
    auto postcondition = ScopeGuard{[&] {
        outstanding_blocks_.erase(it);
        process_block_queue_.pop();
    }};
    const auto& blockHash = it->first.get();
    const auto pBlock = it->second.get();

    if (false == bool(pBlock)) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" invalid block ")(
            blockHash.asHex())
            .Flush();
        auto& vector = blocks_to_request_;
        vector.emplace(vector.begin(), blockHash);

        return;
    }

    const auto& block = *pBlock;
    auto tested = WalletDatabase::MatchingIndices{};
    auto [elements, utxos, targets, outpoints] =
        get_block_targets(blockHash, tested);
    const auto pFilter = filters.LoadFilter(filter_type_, blockHash);

    OT_ASSERT(pFilter);

    const auto& filter = *pFilter;
    auto potential = Patterns{};

    for (const auto& it : filter.Match(targets)) {
        // NOTE GCS::Match returns const_iterators to items in the input vector
        const auto pos = std::distance(targets.cbegin(), it);
        auto& [id, element] = elements.at(pos);
        potential.emplace_back(std::move(id), std::move(element));
    }

    const auto confirmed =
        block.FindMatches(filter_type_, outpoints, potential);
    const auto& [utxo, general] = confirmed;
    const auto& oracle = node_.HeaderOracleInternal();
    const auto pHeader = oracle.LoadHeader(blockHash);

    OT_ASSERT(pHeader);

    const auto& header = *pHeader;
    const auto position = header.Position();
    handle_confirmed_matches(block, position, confirmed);
    const auto [balance, unconfirmed] = db_.GetBalance();
    LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" block ")(
        block.ID().asHex())(" processed in ")(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start)
            .count())(" milliseconds. ")(general.size())(" of ")(
        potential.size())(" potential matches confirmed. Wallet balance is: ")(
        unconfirmed)(" (")(balance)(" confirmed)")
        .Flush();
    db_.SubchainMatchBlock(index_, tested, blockHash.Bytes());

    if (0 < general.size()) {
        // Re-scan this block because new keys may have been generated
        const auto height = std::max(header.Height() - 1, block::Height{0});
        last_scanned_ = block::Position{height, oracle.BestHash(height)};
    }
}

auto SubchainStateData::queue_work(const Task task, const char* log) noexcept
    -> bool
{
    constexpr auto limit = std::chrono::minutes{1};
    const auto start = Clock::now();

    while (job_counter_.limited()) {
        Sleep(std::chrono::microseconds(100));

        if ((Clock::now() - start) > limit) { OT_FAIL; }
    }

    using Pool = api::internal::ThreadPool;
    auto work = Pool::MakeWork(
        api_.Network().ZeroMQ(), value(Pool::Work::BlockchainWallet));
    work->AddFrame(task);
    work->AddFrame(reinterpret_cast<std::uintptr_t>(this));
    running_.store(true);

    if (thread_pool_.Send(work)) {
        LogDebug(OT_METHOD)(__FUNCTION__)(": ")(name_)(" ")(log)(" job queued")
            .Flush();
        ++job_counter_;
    } else {
        LogDebug(OT_METHOD)(__FUNCTION__)(": ")(name_)(" failed to queue ")(
            log)(" job")
            .Flush();

        running_.store(false);
    }

    return running_.load();
}

auto SubchainStateData::reorg() noexcept -> void
{
    while (false == reorg_.Empty()) {
        reorg_.Next();
        const auto tip = db_.SubchainLastScanned(index_);
        const auto reorg = node_.HeaderOracleInternal().CalculateReorg(tip);
        db_.ReorgTo(id_, subchain_, index_, reorg);
    }

    const auto scannedTarget =
        node_.HeaderOracleInternal()
            .CommonParent(db_.SubchainLastScanned(index_))
            .first;

    if (last_scanned_.has_value()) { last_scanned_ = scannedTarget; }

    blocks_to_request_.clear();
    outstanding_blocks_.clear();

    while (false == process_block_queue_.empty()) {
        process_block_queue_.pop();
    }
}

auto SubchainStateData::report_scan() noexcept -> void
{
    const auto pos = last_scanned_.value_or(null_position_);

    if (pos == last_reported_) { return; }

    if (blocks_to_request_.empty() && process_block_queue_.empty()) {
        crypto_.ReportScan(node_.Chain(), owner_, id_, subchain_, pos);
        last_reported_ = pos;
        db_.SubchainSetLastScanned(index_, pos);
    }
}

auto SubchainStateData::scan() noexcept -> void
{
    const auto start = Clock::now();
    const auto& headers = node_.HeaderOracleInternal();
    const auto& filters = node_.FilterOracleInternal();
    const auto best = headers.BestChain();
    const auto startHeight = std::min(
        best.first,
        last_scanned_.has_value() ? last_scanned_.value().first + 1 : 0);
    const auto first =
        last_scanned_.has_value()
            ? block::Position{startHeight, headers.BestHash(startHeight)}
            : block::Position{0, headers.BestHash(0)};
    const auto stopHeight = std::min(
        std::min(startHeight + 9999, best.first),
        filters.FilterTip(filter_type_).first);
    LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" scanning filters from ")(
        startHeight)(" to ")(stopHeight)
        .Flush();
    const auto [elements, utxos, patterns] = get_account_targets();
    auto highestTested = last_scanned_.value_or(null_position_);
    auto atLeastOnce{false};
    auto blockHash = api_.Factory().Data();
    auto cache = decltype(blocks_to_request_){};

    for (auto i{startHeight}; i <= stopHeight; ++i) {
        blockHash = headers.BestHash(i);
        const auto pFilter = filters.LoadFilterOrResetTip(
            filter_type_, block::Position{i, blockHash});

        if (false == bool(pFilter)) {
            LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(
                " filter at height ")(i)(" not found ")
                .Flush();

            break;
        }

        atLeastOnce = true;
        highestTested.first = i;
        highestTested.second = blockHash;
        const auto& filter = *pFilter;
        auto matches = filter.Match(patterns);
        const auto size{matches.size()};

        if (0 < matches.size()) {
            LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" GCS for block ")(
                blockHash->asHex())(" at height ")(i)(
                " matches at least one of the ")(patterns.size())(
                " target elements for ")(id_)
                .Flush();
            const auto [untested, retest] = get_block_targets(blockHash, utxos);
            matches = filter.Match(retest);
            LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" ")(
                matches.size())(" of ")(size)(" matches are new")
                .Flush();

            if (0 < matches.size()) {
                cache.emplace_back(std::move(blockHash));
            }
        }
    }

    if (atLeastOnce) {
        const auto count = cache.size();
        LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(" found ")(count)(
            " potential matches between blocks ")(startHeight)(" and ")(
            highestTested.first)(" in ")(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - start)
                .count())(" milliseconds")
            .Flush();
        std::move(
            cache.begin(), cache.end(), std::back_inserter(blocks_to_request_));
        last_scanned_ = std::move(highestTested);
    } else {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": ")(name_)(
            " scan interrupted due to missing filter")
            .Flush();
    }
}

auto SubchainStateData::set_key_data(
    block::bitcoin::Transaction& tx) const noexcept -> void
{
    const auto keys = tx.Keys();
    auto data = block::bitcoin::Transaction::KeyData{};

    for (const auto& key : keys) {
        data.try_emplace(
            key, crypto_.SenderContact(key), crypto_.RecipientContact(key));
    }

    tx.SetKeyData(data);
}

auto SubchainStateData::state_machine() noexcept -> bool
{
    if (running_) {
        LogTrace(OT_METHOD)(__FUNCTION__)(": ")(name_)(" task is running")
            .Flush();

        return false;
    }

    if (check_reorg()) { return false; }
    if (check_blocks()) { return false; }
    if (check_index()) { return false; }
    if (check_scan()) { return false; }
    if (check_process()) { return false; }

    return false;
}

SubchainStateData::~SubchainStateData()
{
    while (0 < job_counter_) { Sleep(std::chrono::microseconds(100)); }
}
}  // namespace opentxs::blockchain::node::wallet
