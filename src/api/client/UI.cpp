// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"       // IWYU pragma: associated
#include "1_Internal.hpp"     // IWYU pragma: associated
#include "api/client/UI.hpp"  // IWYU pragma: associated

#include <functional>
#include <map>
#include <memory>
#include <tuple>

#include "internal/api/client/Client.hpp"
#include "internal/api/client/Factory.hpp"
#include "internal/ui/UI.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/protobuf/ContactEnums.pb.h"
#include "opentxs/ui/AccountActivity.hpp"
#include "opentxs/ui/AccountList.hpp"
#include "opentxs/ui/AccountSummary.hpp"
#include "opentxs/ui/ActivitySummary.hpp"
#include "opentxs/ui/ActivityThread.hpp"
#if OT_BLOCKCHAIN
#include "opentxs/ui/BlockchainSelection.hpp"
#endif  // OT_BLOCKCHAIN
#include "opentxs/ui/Contact.hpp"
#include "opentxs/ui/ContactList.hpp"
#include "opentxs/ui/MessagableList.hpp"
#include "opentxs/ui/PayableList.hpp"
#include "opentxs/ui/Profile.hpp"
#include "opentxs/ui/UnitList.hpp"
#include "ui/AccountActivity.hpp"
#include "ui/AccountList.hpp"
#include "ui/AccountSummary.hpp"
#include "ui/ActivitySummary.hpp"
#include "ui/ActivityThread.hpp"
#if OT_BLOCKCHAIN
#include "ui/BlockchainSelection.hpp"
#endif  // OT_BLOCKCHAIN
#include "ui/Contact.hpp"
#include "ui/ContactList.hpp"
#include "ui/MessagableList.hpp"
#include "ui/PayableList.hpp"
#include "ui/Profile.hpp"
#include "ui/UnitList.hpp"

//#define OT_METHOD "opentxs::api::implementation::UI"

namespace opentxs::factory
{
auto UI(
    const api::client::internal::Manager& api,
#if OT_BLOCKCHAIN
    const api::client::internal::Blockchain& blockchain,
#endif  // OT_BLOCKCHAIN
    const Flag& running
#if OT_QT
    ,
    const bool qt
#endif
    ) noexcept -> std::unique_ptr<api::client::internal::UI>
{
    using ReturnType = api::client::implementation::UI;

    return std::make_unique<ReturnType>(
        api,
#if OT_BLOCKCHAIN
        blockchain,
#endif  // OT_BLOCKCHAIN
        running
#if OT_QT
        ,
        qt
#endif
    );
}
}  // namespace opentxs::factory

namespace opentxs::api::client::implementation
{
UI::UI(
    const api::client::internal::Manager& api,
#if OT_BLOCKCHAIN
    const api::client::internal::Blockchain& blockchain,
#endif  // OT_BLOCKCHAIN
    const Flag& running
#if OT_QT
    ,
    const bool qt
#endif
    ) noexcept
    : api_(api)
#if OT_BLOCKCHAIN
    , blockchain_(blockchain)
#endif  // OT_BLOCKCHAIN
    , running_(running)
#if OT_QT
    , enable_qt_(qt)
#endif
    , accounts_()
    , account_lists_()
    , account_summaries_()
    , activity_summaries_()
    , contacts_()
    , contact_lists_()
    , messagable_lists_()
    , payable_lists_()
    , activity_threads_()
    , profiles_()
    , unit_lists_()
#if OT_BLOCKCHAIN
    , blockchain_selection_()
#endif  // OT_BLOCKCHAIN
#if OT_QT
    , blank_()
    , accounts_qt_()
    , account_lists_qt_()
    , account_summaries_qt_()
    , activity_summaries_qt_()
    , contacts_qt_()
    , contact_lists_qt_()
    , messagable_lists_qt_()
    , payable_lists_qt_()
    , activity_threads_qt_()
    , profiles_qt_()
    , unit_lists_qt_()
#if OT_BLOCKCHAIN
    , blockchain_selection_qt_()
#endif  // OT_BLOCKCHAIN
#endif  // OT_QT
    , update_manager_(api_)
{
    // WARNING: do not access api_.Wallet() during construction
}

UI::UpdateManager::UpdateManager(
    const api::client::internal::Manager& api) noexcept
    : api_(api)
    , lock_()
    , map_()
    , publisher_(api.ZeroMQ().PublishSocket())
    , pipeline_(api.ZeroMQ().Pipeline(api, [this](auto& in) { pipeline(in); }))
{
    publisher_->Start(api_.Endpoints().WidgetUpdate());
}

#if OT_QT
auto UI::Blank::get(const std::size_t columns) noexcept -> ui::BlankModel*
{
    Lock lock(lock_);

    {
        auto it = map_.find(columns);

        if (map_.end() != it) { return &(it->second); }
    }

    return &(map_.emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(columns),
                     std::forward_as_tuple(columns))
                 .first->second);
}
#endif  // OT_QT

auto UI::UpdateManager::ActivateUICallback(
    const Identifier& widget) const noexcept -> void
{
    pipeline_->Push(widget.str());
}

auto UI::UpdateManager::pipeline(zmq::Message& in) noexcept -> void
{
    if (0 == in.size()) { return; }

    const auto& frame = in.at(0);
    const auto id = api_.Factory().Identifier(frame);
    LogTrace("opentxs::api::implementation::UI::UpdateManager::")(__FUNCTION__)(
        ": Widget ")(id->str())(" updated.")
        .Flush();
    Lock lock(lock_);
    auto it = map_.find(id);

    if (map_.end() == it) { return; }

    const auto& callbacks = it->second;

    for (const auto& cb : callbacks) {
        if (cb) { cb(); }
    }
}

auto UI::UpdateManager::RegisterUICallback(
    const Identifier& widget,
    const SimpleCallback& cb) const noexcept -> void
{
    if (cb) {
        Lock lock(lock_);
        map_[widget].emplace_back(cb);
    }
}

auto UI::account_activity(
    const Lock& lock,
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const SimpleCallback& cb) const noexcept -> AccountActivityMap::mapped_type&
{
    auto key = AccountActivityKey{nymID, accountID};
    auto it = accounts_.find(key);
#if OT_BLOCKCHAIN
    const auto chain = is_blockchain_account(accountID);
#endif  // OT_BLOCKCHAIN

    if (accounts_.end() == it) {
        it = accounts_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(
#if OT_BLOCKCHAIN
                         (chain.has_value()
                              ? opentxs::factory::BlockchainAccountActivityModel
                              : opentxs::factory::AccountActivityModel)
#else   // OT_BLOCKCHAIN
                         (opentxs::factory::AccountActivityModel)
#endif  // OT_BLOCKCHAIN
                             (api_,
                              nymID,
                              accountID,
#if OT_QT
                              enable_qt_,
#endif  // OT_QT
                              cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::AccountActivity(
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const SimpleCallback cb) const noexcept -> const ui::AccountActivity&
{
    Lock lock(lock_);

    return *account_activity(lock, nymID, accountID, cb);
}

#if OT_QT
auto UI::AccountActivityQt(
    const identifier::Nym& nymID,
    const Identifier& accountID,
    const SimpleCallback cb) const noexcept -> ui::AccountActivityQt*
{
    Lock lock(lock_);
    auto key = AccountActivityKey{nymID, accountID};
    auto it = accounts_qt_.find(key);

    if (accounts_qt_.end() == it) {
        it = accounts_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::AccountActivityQtModel(
                         *account_activity(lock, nymID, accountID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::account_list(
    const Lock& lock,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) const noexcept -> AccountListMap::mapped_type&
{
    auto key = AccountListKey{nymID};
    auto it = account_lists_.find(key);

    if (account_lists_.end() == it) {
        it = account_lists_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(opentxs::factory::AccountListModel(
                         api_,
                         nymID,
#if OT_QT
                         enable_qt_,
#endif  // OT_QT
                         cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::AccountList(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> const ui::AccountList&
{
    Lock lock(lock_);

    return *account_list(lock, nymID, cb);
}

#if OT_QT
auto UI::AccountListQt(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> ui::AccountListQt*
{
    Lock lock(lock_);
    auto key = AccountListKey{nymID};
    auto it = account_lists_qt_.find(key);

    if (account_lists_qt_.end() == it) {
        it = account_lists_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::AccountListQtModel(
                         *account_list(lock, nymID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::account_summary(
    const Lock& lock,
    const identifier::Nym& nymID,
    const proto::ContactItemType currency,
    const SimpleCallback& cb) const noexcept -> AccountSummaryMap::mapped_type&
{
    auto key = AccountSummaryKey{nymID, currency};
    auto it = account_summaries_.find(key);

    if (account_summaries_.end() == it) {
        it =
            account_summaries_
                .emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(std::move(key)),
                    std::forward_as_tuple(opentxs::factory::AccountSummaryModel(
                        api_,
                        nymID,
                        currency,
#if OT_QT
                        enable_qt_,
#endif  // OT_QT
                        cb)))
                .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::AccountSummary(
    const identifier::Nym& nymID,
    const proto::ContactItemType currency,
    const SimpleCallback cb) const noexcept -> const ui::AccountSummary&
{
    Lock lock(lock_);

    return *account_summary(lock, nymID, currency, cb);
}

#if OT_QT
auto UI::AccountSummaryQt(
    const identifier::Nym& nymID,
    const proto::ContactItemType currency,
    const SimpleCallback cb) const noexcept -> ui::AccountSummaryQt*
{
    Lock lock(lock_);
    auto key = AccountSummaryKey{nymID, currency};
    auto it = account_summaries_qt_.find(key);

    if (account_summaries_qt_.end() == it) {
        it = account_summaries_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::AccountSummaryQtModel(
                         *account_summary(lock, nymID, currency, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::activity_summary(
    const Lock& lock,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) const noexcept -> ActivitySummaryMap::mapped_type&
{
    auto key = ActivitySummaryKey{nymID};
    auto it = activity_summaries_.find(key);

    if (activity_summaries_.end() == it) {
        it = activity_summaries_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(
                         opentxs::factory::ActivitySummaryModel(
                             api_,
                             running_,
                             nymID,
#if OT_QT
                             enable_qt_,
#endif  // OT_QT
                             cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::ActivitySummary(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> const ui::ActivitySummary&
{
    Lock lock(lock_);

    return *activity_summary(lock, nymID, cb);
}

#if OT_QT
auto UI::ActivitySummaryQt(
    const identifier::Nym& nymID,
    const SimpleCallback cb) const noexcept -> ui::ActivitySummaryQt*
{
    Lock lock(lock_);
    auto key = ActivitySummaryKey{nymID};
    auto it = activity_summaries_qt_.find(key);

    if (activity_summaries_qt_.end() == it) {
        it = activity_summaries_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::ActivitySummaryQtModel(
                         *activity_summary(lock, nymID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::activity_thread(
    const Lock& lock,
    const identifier::Nym& nymID,
    const Identifier& threadID,
    const SimpleCallback& cb) const noexcept -> ActivityThreadMap::mapped_type&
{
    auto key = ActivityThreadKey{nymID, threadID};
    auto it = activity_threads_.find(key);

    if (activity_threads_.end() == it) {
        it =
            activity_threads_
                .emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(std::move(key)),
                    std::forward_as_tuple(opentxs::factory::ActivityThreadModel(
                        api_,
                        nymID,
                        threadID,
#if OT_QT
                        enable_qt_,
#endif  // OT_QT
                        cb)))
                .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::ActivityThread(
    const identifier::Nym& nymID,
    const Identifier& threadID,
    const SimpleCallback cb) const noexcept -> const ui::ActivityThread&
{
    Lock lock(lock_);

    return *activity_thread(lock, nymID, threadID, cb);
}

#if OT_QT
auto UI::ActivityThreadQt(
    const identifier::Nym& nymID,
    const Identifier& threadID,
    const SimpleCallback cb) const noexcept -> ui::ActivityThreadQt*
{
    Lock lock(lock_);
    auto key = ActivityThreadKey{nymID, threadID};
    auto it = activity_threads_qt_.find(key);

    if (activity_threads_qt_.end() == it) {
        it = activity_threads_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::ActivityThreadQtModel(
                         *activity_thread(lock, nymID, threadID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

#if OT_BLOCKCHAIN
auto UI::BlockchainAccountID(
    const opentxs::blockchain::Type chain) const noexcept -> const Identifier&
{
    return ui::AccountID(api_, chain);
}

auto UI::BlockchainAccountToChain(const Identifier& account) const noexcept
    -> opentxs::blockchain::Type
{
    return ui::Chain(api_, account);
}

auto UI::BlockchainNotaryID(const opentxs::blockchain::Type chain)
    const noexcept -> const identifier::Server&
{
    return ui::NotaryID(api_, chain);
}

auto UI::BlockchainSelection() const noexcept -> const ui::BlockchainSelection&
{
    OT_ASSERT(blockchain_selection_);

    return *blockchain_selection_;
}

auto UI::BlockchainUnitID(const opentxs::blockchain::Type chain) const noexcept
    -> const identifier::UnitDefinition&
{
    return ui::UnitID(api_, chain);
}

#endif  // OT_BLOCKCHAIN

auto UI::contact(
    const Lock& lock,
    const Identifier& contactID,
    const SimpleCallback& cb) const noexcept -> ContactMap::mapped_type&
{
    auto key = ContactKey{contactID};
    auto it = contacts_.find(key);

    if (contacts_.end() == it) {
        it = contacts_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(opentxs::factory::ContactModel(
                         api_,
                         contactID,
#if OT_QT
                         enable_qt_,
#endif  // OT_QT
                         cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::Contact(const Identifier& contactID, const SimpleCallback cb)
    const noexcept -> const ui::Contact&
{
    Lock lock(lock_);

    return *contact(lock, contactID, cb);
}

#if OT_QT
auto UI::ContactQt(const Identifier& contactID, const SimpleCallback cb)
    const noexcept -> ui::ContactQt*
{
    Lock lock(lock_);
    auto key = ContactKey{contactID};
    auto it = contacts_qt_.find(key);

    if (contacts_qt_.end() == it) {
        it = contacts_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::ContactQtModel(
                         *contact(lock, contactID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::contact_list(
    const Lock& lock,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) const noexcept -> ContactListMap::mapped_type&
{
    auto key = ContactListKey{nymID};
    auto it = contact_lists_.find(key);

    if (contact_lists_.end() == it) {
        it = contact_lists_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(opentxs::factory::ContactListModel(
                         api_,
                         nymID,
#if OT_QT
                         enable_qt_,
#endif  // OT_QT
                         cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::ContactList(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> const ui::ContactList&
{
    Lock lock(lock_);

    return *contact_list(lock, nymID, cb);
}

#if OT_QT
auto UI::ContactListQt(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> ui::ContactListQt*
{
    Lock lock(lock_);
    auto key = ContactListKey{nymID};
    auto it = contact_lists_qt_.find(key);

    if (contact_lists_qt_.end() == it) {
        it = contact_lists_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::ContactListQtModel(
                         *contact_list(lock, nymID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::DisplayScales(const proto::ContactItemType currency) const noexcept
    -> display::ScaleMap
{
    // FIXME

    return {};
}

auto UI::DisplayScales(const opentxs::blockchain::Type currency) const noexcept
    -> display::ScaleMap
{
    // FIXME

    return {};
}

auto UI::DisplayScales(const Identifier& account) const noexcept
    -> display::ScaleMap
{
    // FIXME

    return {};
}

#if OT_QT
auto UI::DisplayScalesQt(const Identifier& account) const noexcept
    -> ui::DisplayScaleListQt*
{
    // FIXME

    return {};
}
#endif  // OT_QT

auto UI::Init() noexcept -> void
{
#if OT_BLOCKCHAIN
    const_cast<BlockchainSelectionType&>(blockchain_selection_) =
        factory::BlockchainSelectionModel(
            api_,
            blockchain_
#if OT_QT
            ,
            enable_qt_
#endif  // OT_QT
        );

    OT_ASSERT(blockchain_selection_);

#if OT_QT
    const_cast<BlockchainSelectionQtType&>(blockchain_selection_qt_) =
        std::make_shared<ui::BlockchainSelectionQt>(*blockchain_selection_);

    OT_ASSERT(blockchain_selection_qt_);
#endif  // OT_QT

    blockchain_selection_->init();
#endif  // OT_BLOCKCHAIN
}

#if OT_BLOCKCHAIN
auto UI::is_blockchain_account(const Identifier& id) const noexcept
    -> std::optional<opentxs::blockchain::Type>
{
    const auto type = ui::Chain(api_, id);

    if (opentxs::blockchain::Type::Unknown == type) { return {}; }

    return type;
}
#endif  // OT_BLOCKCHAIN

auto UI::messagable_list(
    const Lock& lock,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) const noexcept -> MessagableListMap::mapped_type&
{
    auto key = MessagableListKey{nymID};
    auto it = messagable_lists_.find(key);

    if (messagable_lists_.end() == it) {
        it =
            messagable_lists_
                .emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(std::move(key)),
                    std::forward_as_tuple(opentxs::factory::MessagableListModel(
                        api_,
                        nymID,
#if OT_QT
                        enable_qt_,
#endif  // OT_QT
                        cb)))
                .first;
    }

    return it->second;
}

auto UI::MessagableList(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> const ui::MessagableList&
{
    Lock lock(lock_);

    return *messagable_list(lock, nymID, cb);
}

#if OT_QT
auto UI::MessagableListQt(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> ui::MessagableListQt*
{
    Lock lock(lock_);
    auto key = MessagableListKey{nymID};
    auto it = messagable_lists_qt_.find(key);

    if (messagable_lists_qt_.end() == it) {
        it = messagable_lists_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::MessagableListQtModel(
                         *messagable_list(lock, nymID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::payable_list(
    const Lock& lock,
    const identifier::Nym& nymID,
    const proto::ContactItemType currency,
    const SimpleCallback& cb) const noexcept -> PayableListMap::mapped_type&
{
    auto key = PayableListKey{nymID, currency};
    auto it = payable_lists_.find(key);

    if (payable_lists_.end() == it) {
        it = payable_lists_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(opentxs::factory::PayableListModel(
                         api_,
                         nymID,
                         currency,
#if OT_QT
                         enable_qt_,
#endif  // OT_QT
                         cb)))
                 .first;
    }

    return it->second;
}

auto UI::PayableList(
    const identifier::Nym& nymID,
    proto::ContactItemType currency,
    const SimpleCallback cb) const noexcept -> const ui::PayableList&
{
    Lock lock(lock_);

    return *payable_list(lock, nymID, currency, cb);
}

#if OT_QT
auto UI::PayableListQt(
    const identifier::Nym& nymID,
    proto::ContactItemType currency,
    const SimpleCallback cb) const noexcept -> ui::PayableListQt*
{
    Lock lock(lock_);
    auto key = PayableListKey{nymID, currency};
    auto it = payable_lists_qt_.find(key);

    if (payable_lists_qt_.end() == it) {
        it = payable_lists_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::PayableListQtModel(
                         *payable_list(lock, nymID, currency, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::profile(
    const Lock& lock,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) const noexcept -> ProfileMap::mapped_type&
{
    auto key = ProfileKey{nymID};
    auto it = profiles_.find(key);

    if (profiles_.end() == it) {
        it = profiles_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(opentxs::factory::ProfileModel(
                         api_,
                         nymID,
#if OT_QT
                         enable_qt_,
#endif  // OT_QT
                         cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::Profile(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> const ui::Profile&
{
    Lock lock(lock_);

    return *profile(lock, nymID, cb);
}

#if OT_QT
auto UI::ProfileQt(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> ui::ProfileQt*
{
    Lock lock(lock_);
    auto key = ProfileKey{nymID};
    auto it = profiles_qt_.find(key);

    if (profiles_qt_.end() == it) {
        it =
            profiles_qt_
                .emplace(
                    std::move(key),
                    opentxs::factory::ProfileQtModel(*profile(lock, nymID, cb)))
                .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif

auto UI::unit_list(
    const Lock& lock,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) const noexcept -> UnitListMap::mapped_type&
{
    auto key = UnitListKey{nymID};
    auto it = unit_lists_.find(key);

    if (unit_lists_.end() == it) {
        it = unit_lists_
                 .emplace(
                     std::piecewise_construct,
                     std::forward_as_tuple(std::move(key)),
                     std::forward_as_tuple(opentxs::factory::UnitListModel(
                         api_,
                         nymID,
#if OT_QT
                         enable_qt_,
#endif  // OT_QT
                         cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second;
}

auto UI::UnitList(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> const ui::UnitList&
{
    Lock lock(lock_);

    return *unit_list(lock, nymID, cb);
}

#if OT_QT
auto UI::UnitListQt(const identifier::Nym& nymID, const SimpleCallback cb)
    const noexcept -> ui::UnitListQt*
{
    Lock lock(lock_);
    auto key = UnitListKey{nymID};
    auto it = unit_lists_qt_.find(key);

    if (unit_lists_qt_.end() == it) {
        it = unit_lists_qt_
                 .emplace(
                     std::move(key),
                     opentxs::factory::UnitListQtModel(
                         *unit_list(lock, nymID, cb)))
                 .first;

        OT_ASSERT(it->second);
    }

    return it->second.get();
}
#endif
}  // namespace opentxs::api::client::implementation
