// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: private
// IWYU pragma: friend ".*src/api/client/UI.cpp"

#pragma once

#include <iosfwd>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "internal/api/client/Client.hpp"
#include "opentxs/Forward.hpp"
#include "opentxs/Proto.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/api/client/UI.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/identifier/Server.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/socket/Publish.hpp"
#include "opentxs/protobuf/ContactEnums.pb.h"
#include "opentxs/ui/AccountActivity.hpp"
#include "opentxs/ui/AccountList.hpp"
#include "opentxs/ui/AccountSummary.hpp"
#include "opentxs/ui/ActivitySummary.hpp"
#include "opentxs/ui/ActivityThread.hpp"
#include "opentxs/ui/BlockchainSelection.hpp"
#include "opentxs/ui/Contact.hpp"
#include "opentxs/ui/ContactList.hpp"
#include "opentxs/ui/List.hpp"
#include "opentxs/ui/MessagableList.hpp"
#include "opentxs/ui/PayableList.hpp"
#include "opentxs/ui/Profile.hpp"
#include "opentxs/ui/UnitList.hpp"

namespace opentxs
{
namespace api
{
namespace client
{
namespace internal
{
struct Manager;
}  // namespace internal
}  // namespace client
}  // namespace api

namespace network
{
namespace zeromq
{
class Message;
}  // namespace zeromq
}  // namespace network

namespace ui
{
namespace implementation
{
class AccountActivity;
class AccountList;
class AccountSummary;
class ActivitySummary;
class ActivityThread;
class BlockchainSelection;
class Contact;
class ContactList;
class MessagableList;
class PayableList;
class Profile;
class UnitList;
}  // namespace implementation
}  // namespace ui

class Flag;
}  // namespace opentxs

namespace zmq = opentxs::network::zeromq;

namespace opentxs::api::client::implementation
{
class UI final : public internal::UI, Lockable
{
public:
    auto AccountActivity(
        const identifier::Nym& nymID,
        const Identifier& accountID,
        const SimpleCallback cb) const noexcept
        -> const ui::AccountActivity& final;
    auto AccountList(const identifier::Nym& nym, const SimpleCallback cb)
        const noexcept -> const ui::AccountList& final;
    auto AccountSummary(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback cb) const noexcept
        -> const ui::AccountSummary& final;
    auto ActivateUICallback(const Identifier& widget) const noexcept
        -> void final
    {
        update_manager_.ActivateUICallback(widget);
    }
    auto ActivitySummary(const identifier::Nym& nymID, const SimpleCallback cb)
        const noexcept -> const ui::ActivitySummary& final;
    auto ActivityThread(
        const identifier::Nym& nymID,
        const Identifier& threadID,
        const SimpleCallback cb) const noexcept
        -> const ui::ActivityThread& final;
#if OT_BLOCKCHAIN
    auto BlockchainAccountID(const opentxs::blockchain::Type chain)
        const noexcept -> const Identifier& final;
    auto BlockchainAccountToChain(const Identifier& account) const noexcept
        -> opentxs::blockchain::Type final;
    auto BlockchainNotaryID(const opentxs::blockchain::Type chain)
        const noexcept -> const identifier::Server& final;
    auto BlockchainSelection() const noexcept
        -> const ui::BlockchainSelection& final;
    auto BlockchainUnitID(const opentxs::blockchain::Type chain) const noexcept
        -> const identifier::UnitDefinition& final;
#endif  // OT_BLOCKCHAIN
    auto Contact(const Identifier& contactID, const SimpleCallback cb)
        const noexcept -> const ui::Contact& final;
    auto ContactList(const identifier::Nym& nymID, const SimpleCallback cb)
        const noexcept -> const ui::ContactList& final;
    auto DisplayScales(const proto::ContactItemType currency) const noexcept
        -> display::ScaleMap final;
    auto DisplayScales(const opentxs::blockchain::Type currency) const noexcept
        -> display::ScaleMap final;
    auto DisplayScales(const Identifier& account) const noexcept
        -> display::ScaleMap final;
    auto MessagableList(const identifier::Nym& nymID, const SimpleCallback cb)
        const noexcept -> const ui::MessagableList& final;
    auto PayableList(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback cb) const noexcept -> const ui::PayableList& final;
    auto Profile(const identifier::Nym& nymID, const SimpleCallback cb)
        const noexcept -> const ui::Profile& final;
    auto RegisterUICallback(const Identifier& widget, const SimpleCallback& cb)
        const noexcept -> void final
    {
        update_manager_.RegisterUICallback(widget, cb);
    }
    auto UnitList(const identifier::Nym& nym, const SimpleCallback cb)
        const noexcept -> const ui::UnitList& final;

#if OT_QT
    auto AccountActivityQt(
        const identifier::Nym& nymID,
        const Identifier& accountID,
        const SimpleCallback cb) const noexcept -> ui::AccountActivityQt* final;
    auto AccountListQt(const identifier::Nym& nym, const SimpleCallback cb)
        const noexcept -> ui::AccountListQt* final;
    auto AccountSummaryQt(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback cb) const noexcept -> ui::AccountSummaryQt* final;
    auto ActivitySummaryQt(
        const identifier::Nym& nymID,
        const SimpleCallback cb) const noexcept -> ui::ActivitySummaryQt* final;
    auto ActivityThreadQt(
        const identifier::Nym& nymID,
        const Identifier& threadID,
        const SimpleCallback cb) const noexcept -> ui::ActivityThreadQt* final;
    auto BlankModel(const std::size_t columns) const noexcept
        -> QAbstractItemModel* final
    {
        return blank_.get(columns);
    }
#if OT_BLOCKCHAIN
    auto BlockchainSelectionQt() const noexcept
        -> ui::BlockchainSelectionQt* final
    {
        return blockchain_selection_qt_.get();
    }
#endif  // OT_BLOCKCHAIN
    auto ContactQt(const Identifier& contactID, const SimpleCallback cb)
        const noexcept -> ui::ContactQt* final;
    auto ContactListQt(const identifier::Nym& nymID, const SimpleCallback cb)
        const noexcept -> ui::ContactListQt* final;
    auto DisplayScalesQt(const Identifier& account) const noexcept
        -> ui::DisplayScaleListQt* final;
    auto MessagableListQt(const identifier::Nym& nymID, const SimpleCallback cb)
        const noexcept -> ui::MessagableListQt* final;
    auto PayableListQt(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback cb) const noexcept -> ui::PayableListQt* final;
    auto ProfileQt(const identifier::Nym& nymID, const SimpleCallback cb)
        const noexcept -> ui::ProfileQt* final;
    auto UnitListQt(const identifier::Nym& nym, const SimpleCallback cb)
        const noexcept -> ui::UnitListQt* final;
#endif  // OT_QT

    auto Init() noexcept -> void final;

    UI(const api::client::internal::Manager& api,
#if OT_BLOCKCHAIN
       const api::client::internal::Blockchain& blockchain,
#endif  // OT_BLOCKCHAIN
       const Flag& running
#if OT_QT
       ,
       const bool qt
#endif  // OT_QT
       )
    noexcept;

    ~UI() final = default;

private:
    /** NymID, AccountID */
    using AccountActivityKey = std::pair<OTNymID, OTIdentifier>;
    using AccountListKey = OTNymID;
    /** NymID, currency*/
    using AccountSummaryKey = std::pair<OTNymID, proto::ContactItemType>;
    using ActivitySummaryKey = OTNymID;
    using ActivityThreadKey = std::pair<OTNymID, OTIdentifier>;
    using ContactKey = OTIdentifier;
    using ContactListKey = OTNymID;
    using MessagableListKey = OTNymID;
    /** NymID, currency*/
    using PayableListKey = std::pair<OTNymID, proto::ContactItemType>;
    using ProfileKey = OTNymID;
    using UnitListKey = OTNymID;

    using AccountActivityValue =
        std::unique_ptr<ui::implementation::AccountActivity>;
    using AccountListValue = std::unique_ptr<ui::implementation::AccountList>;
    using AccountSummaryValue =
        std::unique_ptr<ui::implementation::AccountSummary>;
    using ActivitySummaryValue =
        std::unique_ptr<ui::implementation::ActivitySummary>;
    using ActivityThreadValue =
        std::unique_ptr<ui::implementation::ActivityThread>;
    using ContactValue = std::unique_ptr<ui::implementation::Contact>;
    using ContactListValue = std::unique_ptr<ui::implementation::ContactList>;
    using MessagableListValue =
        std::unique_ptr<ui::implementation::MessagableList>;
    using PayableListValue = std::unique_ptr<ui::implementation::PayableList>;
    using ProfileValue = std::unique_ptr<ui::implementation::Profile>;
    using AccountActivityMap =
        std::map<AccountActivityKey, AccountActivityValue>;
    using AccountListMap = std::map<AccountListKey, AccountListValue>;
    using AccountSummaryMap = std::map<AccountSummaryKey, AccountSummaryValue>;
    using ActivitySummaryMap =
        std::map<ActivitySummaryKey, ActivitySummaryValue>;
    using ActivityThreadMap = std::map<ActivityThreadKey, ActivityThreadValue>;
    using ContactMap = std::map<ContactKey, ContactValue>;
    using ContactListMap = std::map<ContactListKey, ContactListValue>;
    using MessagableListMap = std::map<MessagableListKey, MessagableListValue>;
    using PayableListMap = std::map<PayableListKey, PayableListValue>;
    using ProfileMap = std::map<ProfileKey, ProfileValue>;
    using UnitListValue = std::unique_ptr<ui::implementation::UnitList>;
    using UnitListMap = std::map<UnitListKey, UnitListValue>;
#if OT_BLOCKCHAIN
    using BlockchainSelectionType =
        std::shared_ptr<ui::implementation::BlockchainSelection>;
#endif  // OT_BLOCKCHAIN

#if OT_QT
    using AccountActivityQtValue = std::unique_ptr<ui::AccountActivityQt>;
    using AccountListQtValue = std::unique_ptr<ui::AccountListQt>;
    using AccountSummaryQtValue = std::unique_ptr<ui::AccountSummaryQt>;
    using ActivitySummaryQtValue = std::unique_ptr<ui::ActivitySummaryQt>;
    using ActivityThreadQtValue = std::unique_ptr<ui::ActivityThreadQt>;
    using ContactQtValue = std::unique_ptr<ui::ContactQt>;
    using ContactListQtValue = std::unique_ptr<ui::ContactListQt>;
    using MessagableListQtValue = std::unique_ptr<ui::MessagableListQt>;
    using PayableListQtValue = std::unique_ptr<ui::PayableListQt>;
    using ProfileQtValue = std::unique_ptr<ui::ProfileQt>;
    using UnitListQtValue = std::unique_ptr<ui::UnitListQt>;
    using AccountActivityQtMap =
        std::map<AccountActivityKey, AccountActivityQtValue>;
    using AccountListQtMap = std::map<AccountListKey, AccountListQtValue>;
    using AccountSummaryQtMap =
        std::map<AccountSummaryKey, AccountSummaryQtValue>;
    using ActivitySummaryQtMap =
        std::map<ActivitySummaryKey, ActivitySummaryQtValue>;
    using ActivityThreadQtMap =
        std::map<ActivityThreadKey, ActivityThreadQtValue>;
    using ContactQtMap = std::map<ContactKey, ContactQtValue>;
    using ContactListQtMap = std::map<ContactListKey, ContactListQtValue>;
    using MessagableListQtMap =
        std::map<MessagableListKey, MessagableListQtValue>;
    using PayableListQtMap = std::map<PayableListKey, PayableListQtValue>;
    using ProfileQtMap = std::map<ProfileKey, ProfileQtValue>;
    using UnitListQtMap = std::map<UnitListKey, UnitListQtValue>;
#if OT_BLOCKCHAIN
    using BlockchainSelectionQtType =
        std::shared_ptr<ui::BlockchainSelectionQt>;
#endif  // OT_BLOCKCHAIN

    struct Blank {
        auto get(const std::size_t columns) noexcept -> ui::BlankModel*;

    private:
        std::mutex lock_{};
        std::map<std::size_t, ui::BlankModel> map_{};
    };
#endif  // OT_QT

    struct UpdateManager {
        auto ActivateUICallback(const Identifier& widget) const noexcept
            -> void;
        auto RegisterUICallback(
            const Identifier& widget,
            const SimpleCallback& cb) const noexcept -> void;

        UpdateManager(const api::client::internal::Manager& api) noexcept;

    private:
        const api::client::internal::Manager& api_;
        mutable std::mutex lock_;
        mutable std::map<OTIdentifier, std::vector<SimpleCallback>> map_;
        OTZMQPublishSocket publisher_;
        OTZMQPipeline pipeline_;

        auto pipeline(zmq::Message& in) noexcept -> void;
    };

    const api::client::internal::Manager& api_;
#if OT_BLOCKCHAIN
    const api::client::internal::Blockchain& blockchain_;
#endif  // OT_BLOCKCHAIN
    const Flag& running_;
#if OT_QT
    const bool enable_qt_;
#endif  // OT_QT
    mutable AccountActivityMap accounts_;
    mutable AccountListMap account_lists_;
    mutable AccountSummaryMap account_summaries_;
    mutable ActivitySummaryMap activity_summaries_;
    mutable ContactMap contacts_;
    mutable ContactListMap contact_lists_;
    mutable MessagableListMap messagable_lists_;
    mutable PayableListMap payable_lists_;
    mutable ActivityThreadMap activity_threads_;
    mutable ProfileMap profiles_;
    mutable UnitListMap unit_lists_;
#if OT_BLOCKCHAIN
    const BlockchainSelectionType blockchain_selection_;
#endif  // OT_BLOCKCHAIN
#if OT_QT
    mutable Blank blank_;
    mutable AccountActivityQtMap accounts_qt_;
    mutable AccountListQtMap account_lists_qt_;
    mutable AccountSummaryQtMap account_summaries_qt_;
    mutable ActivitySummaryQtMap activity_summaries_qt_;
    mutable ContactQtMap contacts_qt_;
    mutable ContactListQtMap contact_lists_qt_;
    mutable MessagableListQtMap messagable_lists_qt_;
    mutable PayableListQtMap payable_lists_qt_;
    mutable ActivityThreadQtMap activity_threads_qt_;
    mutable ProfileQtMap profiles_qt_;
    mutable UnitListQtMap unit_lists_qt_;
#if OT_BLOCKCHAIN
    const BlockchainSelectionQtType blockchain_selection_qt_;
#endif  // OT_BLOCKCHAIN
#endif  // OT_QT
    UpdateManager update_manager_;

    auto account_activity(
        const Lock& lock,
        const identifier::Nym& nymID,
        const Identifier& accountID,
        const SimpleCallback& cb) const noexcept
        -> AccountActivityMap::mapped_type&;
    auto account_list(
        const Lock& lock,
        const identifier::Nym& nymID,
        const SimpleCallback& cb) const noexcept
        -> AccountListMap::mapped_type&;
    auto account_summary(
        const Lock& lock,
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback& cb) const noexcept
        -> AccountSummaryMap::mapped_type&;
    auto activity_summary(
        const Lock& lock,
        const identifier::Nym& nymID,
        const SimpleCallback& cb) const noexcept
        -> ActivitySummaryMap::mapped_type&;
    auto activity_thread(
        const Lock& lock,
        const identifier::Nym& nymID,
        const Identifier& threadID,
        const SimpleCallback& cb) const noexcept
        -> ActivityThreadMap::mapped_type&;
    auto contact(
        const Lock& lock,
        const Identifier& contactID,
        const SimpleCallback& cb) const noexcept -> ContactMap::mapped_type&;
    auto contact_list(
        const Lock& lock,
        const identifier::Nym& nymID,
        const SimpleCallback& cb) const noexcept
        -> ContactListMap::mapped_type&;
#if OT_BLOCKCHAIN
    auto is_blockchain_account(const Identifier& id) const noexcept
        -> std::optional<opentxs::blockchain::Type>;
#endif  // OT_BLOCKCHAIN
    auto messagable_list(
        const Lock& lock,
        const identifier::Nym& nymID,
        const SimpleCallback& cb) const noexcept
        -> MessagableListMap::mapped_type&;
    auto payable_list(
        const Lock& lock,
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback& cb) const noexcept
        -> PayableListMap::mapped_type&;
    auto profile(
        const Lock& lock,
        const identifier::Nym& nymID,
        const SimpleCallback& cb) const noexcept -> ProfileMap::mapped_type&;
    auto unit_list(
        const Lock& lock,
        const identifier::Nym& nymID,
        const SimpleCallback& cb) const noexcept -> UnitListMap::mapped_type&;

    UI() = delete;
    UI(const UI&) = delete;
    UI(UI&&) = delete;
    auto operator=(const UI&) -> UI& = delete;
    auto operator=(UI &&) -> UI& = delete;
};
}  // namespace opentxs::api::client::implementation
