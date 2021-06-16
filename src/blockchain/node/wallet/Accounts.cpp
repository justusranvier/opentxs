// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                         // IWYU pragma: associated
#include "1_Internal.hpp"                       // IWYU pragma: associated
#include "blockchain/node/wallet/Accounts.hpp"  // IWYU pragma: associated

#include <map>
#include <set>
#include <utility>

#include "blockchain/node/wallet/Account.hpp"
#include "blockchain/node/wallet/NotificationStateData.hpp"
#include "blockchain/node/wallet/SubchainStateData.hpp"
#include "internal/api/client/Client.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/blockchain/FilterType.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/core/crypto/PaymentCode.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/protobuf/HDPath.pb.h"
#include "util/Gatekeeper.hpp"
#include "util/JobCounter.hpp"

// #define OT_METHOD "opentxs::blockchain::node::wallet::Accounts::"

namespace opentxs::blockchain::node::wallet
{
struct Accounts::Imp {
    auto Add(const identifier::Nym& nym) noexcept -> bool
    {
        auto ticket = gatekeeper_.get();

        if (ticket) { return false; }

        auto [it, added] = map_.try_emplace(
            nym,
            api_,
            crypto_,
            crypto_.AccountInternal(nym, chain_),
            node_,
            db_,
            thread_pool_,
            filter_type_,
            job_counter_.Allocate(),
            task_finished_);

        if (added) {
            LogNormal("Initializing ")(DisplayString(chain_))(" wallet for ")(
                nym)
                .Flush();
        }

        index_nym(nym);

        return added;
    }
    auto Add(const zmq::Frame& message) noexcept -> bool
    {
        auto id = api_.Factory().NymID();
        id->Assign(message.Bytes());

        if (0 == id->size()) { return false; }

        return Add(id);
    }
    auto Reorg(const block::Position& parent) noexcept -> bool
    {
        auto ticket = gatekeeper_.get();

        if (ticket) { return false; }

        auto output{false};

        for (auto& [nym, account] : map_) { output |= account.reorg(parent); }

        for (auto& [code, account] : payment_codes_) {
            output |= account.reorg_.Queue(parent);
        }

        return output;
    }
    auto shutdown() noexcept -> void
    {
        gatekeeper_.shutdown();
        payment_codes_.clear();

        for (auto& [id, account] : map_) { account.shutdown(); }
    }
    auto state_machine() noexcept -> bool
    {
        auto ticket = gatekeeper_.get();

        if (ticket) { return false; }

        auto output{false};

        for (auto& [code, account] : payment_codes_) {
            output |= account.state_machine();
        }

        for (auto& [nym, account] : map_) { output |= account.state_machine(); }

        return output;
    }

    Imp(const api::Core& api,
        const api::client::internal::Blockchain& crypto,
        const node::internal::Manager& node,
        const node::internal::WalletDatabase& db,
        const network::zeromq::socket::Push& socket,
        const Type chain,
        const SimpleCallback& taskFinished) noexcept
        : api_(api)
        , crypto_(crypto)
        , node_(node)
        , db_(db)
        , thread_pool_(socket)
        , task_finished_(taskFinished)
        , chain_(chain)
        , filter_type_(node_.FilterOracleInternal().DefaultType())
        , job_counter_()
        , map_()
        , pc_counter_(job_counter_.Allocate())
        , payment_codes_()
        , gatekeeper_()
    {
        for (const auto& id : api_.Wallet().LocalNyms()) { Add(id); }
    }

    ~Imp() { shutdown(); }

private:
    using AccountMap = std::map<OTNymID, wallet::Account>;
    using PCMap = std::map<OTIdentifier, NotificationStateData>;

    const api::Core& api_;
    const api::client::internal::Blockchain& crypto_;
    const node::internal::Manager& node_;
    const node::internal::WalletDatabase& db_;
    const network::zeromq::socket::Push& thread_pool_;
    const SimpleCallback& task_finished_;
    const Type chain_;
    const filter::Type filter_type_;
    JobCounter job_counter_;
    AccountMap map_;
    Outstanding pc_counter_;
    PCMap payment_codes_;
    Gatekeeper gatekeeper_;

    auto index_nym(const identifier::Nym& id) noexcept -> void
    {
        const auto pNym = api_.Wallet().Nym(id);

        OT_ASSERT(pNym);

        const auto& nym = *pNym;
        auto code = api_.Factory().PaymentCode(nym.PaymentCode());

        if (3 > code->Version()) { return; }

        LogNormal("Initializing payment code ")(code->asBase58())(" on ")(
            DisplayString(chain_))
            .Flush();
        auto accountID =
            NotificationStateData::calculate_id(api_, chain_, code);
        payment_codes_.try_emplace(
            std::move(accountID),
            api_,
            crypto_,
            node_,
            db_,
            task_finished_,
            pc_counter_,
            thread_pool_,
            filter_type_,
            chain_,
            id,
            std::move(code),
            [&] {
                auto out = proto::HDPath{};
                nym.PaymentCodePath(out);

                return out;
            }());
    }
};

Accounts::Accounts(
    const api::Core& api,
    const api::client::internal::Blockchain& crypto,
    const node::internal::Manager& node,
    const node::internal::WalletDatabase& db,
    const network::zeromq::socket::Push& socket,
    const Type chain,
    const SimpleCallback& taskFinished) noexcept
    : imp_(std::make_unique<
           Imp>(api, crypto, node, db, socket, chain, taskFinished))
{
}

auto Accounts::Add(const identifier::Nym& nym) noexcept -> bool
{
    return imp_->Add(nym);
}

auto Accounts::Add(const zmq::Frame& message) noexcept -> bool
{
    return imp_->Add(message);
}

auto Accounts::Reorg(const block::Position& parent) noexcept -> bool
{
    return imp_->Reorg(parent);
}

auto Accounts::shutdown() noexcept -> void { imp_->shutdown(); }

auto Accounts::state_machine() noexcept -> bool
{
    return imp_->state_machine();
}

Accounts::~Accounts() { imp_->shutdown(); }
}  // namespace opentxs::blockchain::node::wallet
