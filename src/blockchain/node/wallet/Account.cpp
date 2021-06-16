// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                        // IWYU pragma: associated
#include "1_Internal.hpp"                      // IWYU pragma: associated
#include "blockchain/node/wallet/Account.hpp"  // IWYU pragma: associated

#include <map>
#include <utility>

#include "blockchain/node/wallet/DeterministicStateData.hpp"
#include "blockchain/node/wallet/SubchainStateData.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/blockchain/FilterType.hpp"
#include "opentxs/blockchain/crypto/Account.hpp"
#include "opentxs/blockchain/crypto/Deterministic.hpp"
#include "opentxs/blockchain/crypto/HD.hpp"
#include "opentxs/blockchain/crypto/PaymentCode.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"  // IWYU pragma: keep
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/iterator/Bidirectional.hpp"
#include "util/Gatekeeper.hpp"
#include "util/JobCounter.hpp"

#define OT_METHOD "opentxs::blockchain::node::wallet::Account::"

namespace opentxs::blockchain::node::wallet
{
using Subchain = node::internal::WalletDatabase::Subchain;

struct Account::Imp {
    auto reorg(const block::Position& parent) noexcept -> bool
    {
        auto ticket = gatekeeper_.get();

        if (ticket) { return false; }

        auto output{false};

        for (const auto& account : ref_.GetHD()) {
            const auto& id = account.ID();
            LogVerbose(OT_METHOD)(__FUNCTION__)(": Processing HD account ")(id)
                .Flush();
            output |= get(account, Subchain::Internal, internal_)
                          .reorg_.Queue(parent);
            output |= get(account, Subchain::External, external_)
                          .reorg_.Queue(parent);
        }

        for (const auto& account : ref_.GetPaymentCode()) {
            const auto& id = account.ID();
            LogVerbose(OT_METHOD)(__FUNCTION__)(
                ": Processing payment code account ")(id)
                .Flush();
            output |= get(account, Subchain::Outgoing, outgoing_)
                          .reorg_.Queue(parent);
            output |= get(account, Subchain::Incoming, incoming_)
                          .reorg_.Queue(parent);
        }

        return output;
    }
    auto shutdown() noexcept -> void
    {
        gatekeeper_.shutdown();
        internal_.clear();
        external_.clear();
        outgoing_.clear();
        incoming_.clear();
    }
    auto state_machine() noexcept -> bool
    {
        auto ticket = gatekeeper_.get();

        if (ticket) { return false; }

        auto output{false};

        for (const auto& account : ref_.GetHD()) {
            const auto& id = account.ID();
            LogVerbose(OT_METHOD)(__FUNCTION__)(": Processing HD account ")(id)
                .Flush();
            output |=
                get(account, Subchain::Internal, internal_).state_machine();
            output |=
                get(account, Subchain::External, external_).state_machine();
        }

        for (const auto& account : ref_.GetPaymentCode()) {
            const auto& id = account.ID();
            LogVerbose(OT_METHOD)(__FUNCTION__)(
                ": Processing payment code account ")(id)
                .Flush();
            output |=
                get(account, Subchain::Outgoing, outgoing_).state_machine();
            output |=
                get(account, Subchain::Incoming, incoming_).state_machine();
        }

        return output;
    }

    Imp(const api::Core& api,
        const api::client::internal::Blockchain& crypto,
        const BalanceTree& ref,
        const node::internal::Manager& node,
        const node::internal::WalletDatabase& db,
        const zmq::socket::Push& threadPool,
        const filter::Type filter,
        Outstanding&& jobs,
        const SimpleCallback& taskFinished) noexcept
        : api_(api)
        , crypto_(crypto)
        , ref_(ref)
        , node_(node)
        , db_(db)
        , filter_type_(node_.FilterOracleInternal().DefaultType())
        , thread_pool_(threadPool)
        , task_finished_(taskFinished)
        , internal_()
        , external_()
        , outgoing_()
        , incoming_()
        , jobs_(std::move(jobs))
        , gatekeeper_()
    {
        for (const auto& account : ref_.GetHD()) {
            instantiate(account, Subchain::Internal, internal_);
            instantiate(account, Subchain::External, external_);
        }

        for (const auto& account : ref_.GetPaymentCode()) {
            instantiate(account, Subchain::Outgoing, outgoing_);
            instantiate(account, Subchain::Incoming, incoming_);
        }
    }
    ~Imp() { shutdown(); }

private:
    using Map = std::map<OTIdentifier, DeterministicStateData>;

    const api::Core& api_;
    const api::client::internal::Blockchain& crypto_;
    const BalanceTree& ref_;
    const node::internal::Manager& node_;
    const node::internal::WalletDatabase& db_;
    const filter::Type filter_type_;
    const network::zeromq::socket::Push& thread_pool_;
    const SimpleCallback& task_finished_;
    Map internal_;
    Map external_;
    Map outgoing_;
    Map incoming_;
    Outstanding jobs_;
    Gatekeeper gatekeeper_;

    auto get(
        const crypto::Deterministic& account,
        const Subchain subchain,
        Map& map) noexcept -> DeterministicStateData&
    {
        auto it = map.find(account.ID());

        if (map.end() != it) { return it->second; }

        return instantiate(account, subchain, map);
    }
    auto instantiate(
        const crypto::Deterministic& account,
        const Subchain subchain,
        Map& map) noexcept -> DeterministicStateData&
    {
        auto [it, added] = map.try_emplace(
            account.ID(),
            api_,
            crypto_,
            node_,
            db_,
            account,
            task_finished_,
            jobs_,
            thread_pool_,
            filter_type_,
            subchain);

        OT_ASSERT(added);

        return it->second;
    }
};

Account::Account(
    const api::Core& api,
    const api::client::internal::Blockchain& crypto,
    const BalanceTree& ref,
    const node::internal::Manager& node,
    const node::internal::WalletDatabase& db,
    const zmq::socket::Push& threadPool,
    const filter::Type filter,
    Outstanding&& jobs,
    const SimpleCallback& taskFinished) noexcept
    : imp_(std::make_unique<Imp>(
          api,
          crypto,
          ref,
          node,
          db,
          threadPool,
          filter,
          std::move(jobs),
          taskFinished))
{
    OT_ASSERT(imp_);
}

Account::Account(Account&& rhs) noexcept
    : imp_(std::move(rhs.imp_))
{
    OT_ASSERT(imp_);
}

auto Account::reorg(const block::Position& parent) noexcept -> bool
{
    return imp_->reorg(parent);
}

auto Account::shutdown() noexcept -> void { imp_->shutdown(); }

auto Account::state_machine() noexcept -> bool { return imp_->state_machine(); }

Account::~Account() { imp_->shutdown(); }
}  // namespace opentxs::blockchain::node::wallet
