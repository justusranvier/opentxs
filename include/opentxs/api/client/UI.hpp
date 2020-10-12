// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_API_CLIENT_UI_HPP
#define OPENTXS_API_CLIENT_UI_HPP

// IWYU pragma: no_include "opentxs/Proto.hpp"

#include "opentxs/Forward.hpp"  // IWYU pragma: associated

#include "opentxs/Proto.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/display/Types.hpp"
#include "opentxs/protobuf/ContactEnums.pb.h"

#ifdef SWIG
// clang-format off
%extend opentxs::api::client::UI {
    const ui::AccountSummary& AccountSummary(
        const identifier::Nym& nymID,
        const int currency) const noexcept
    {
        return $self->AccountSummary(
            nymID,
            static_cast<opentxs::proto::ContactItemType>(currency));
    }
    const opentxs::ui::PayableList& PayableList(
        const identifier::Nym& nymID,
        const int currency) const noexcept
    {
        return $self->PayableList(
            nymID,
            static_cast<opentxs::proto::ContactItemType>(currency));
    }
}
%ignore opentxs::api::client::UI::AccountSummary;
%ignore opentxs::api::client::UI::PayableList;
// clang-format on
#endif  // SWIG

namespace opentxs
{
namespace ui
{
class AccountActivity;
class AccountList;
class AccountListItem;
class AccountSummary;
class AccountSummaryItem;
class ActivitySummary;
class ActivitySummaryItem;
class ActivityThread;
class ActivityThreadItem;
class BalanceItem;
class BlockchainSelection;
class Contact;
class ContactItem;
class ContactList;
class ContactListItem;
class ContactSection;
class ContactSubsection;
class IssuerItem;
class ListRow;
class MessagableList;
class PayableList;
class PayableListItem;
class Profile;
class ProfileItem;
class ProfileSection;
class ProfileSubsection;
class UnitList;

#if OT_QT
class AccountActivityQt;
class AccountListQt;
class AccountSummaryQt;
class ActivitySummaryQt;
class ActivityThreadQt;
class BlockchainSelectionQt;
class ContactListQt;
class ContactQt;
class DisplayScaleListQt;
class MessagableListQt;
class PayableListQt;
class ProfileQt;
class UnitListQt;
#endif
}  // namespace ui
}  // namespace opentxs

namespace opentxs
{
namespace api
{
namespace client
{
class UI
{
public:
    OPENTXS_EXPORT virtual const ui::AccountActivity& AccountActivity(
        const identifier::Nym& nymID,
        const Identifier& accountID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::AccountList& AccountList(
        const identifier::Nym& nym,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::AccountSummary& AccountSummary(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::ActivitySummary& ActivitySummary(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::ActivityThread& ActivityThread(
        const identifier::Nym& nymID,
        const Identifier& threadID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
#if OT_BLOCKCHAIN
    OPENTXS_EXPORT virtual const Identifier& BlockchainAccountID(
        const opentxs::blockchain::Type chain) const noexcept = 0;
    OPENTXS_EXPORT virtual opentxs::blockchain::Type BlockchainAccountToChain(
        const Identifier& account) const noexcept = 0;
    OPENTXS_EXPORT virtual const identifier::Server& BlockchainNotaryID(
        const opentxs::blockchain::Type chain) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::BlockchainSelection& BlockchainSelection()
        const noexcept = 0;
    OPENTXS_EXPORT virtual const identifier::UnitDefinition& BlockchainUnitID(
        const opentxs::blockchain::Type chain) const noexcept = 0;
#endif  // OT_BLOCKCHAIN
    OPENTXS_EXPORT virtual const ui::Contact& Contact(
        const Identifier& contactID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::ContactList& ContactList(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual display::ScaleMap DisplayScales(
        const proto::ContactItemType currency) const noexcept = 0;
    OPENTXS_EXPORT virtual display::ScaleMap DisplayScales(
        const opentxs::blockchain::Type currency) const noexcept = 0;
    OPENTXS_EXPORT virtual display::ScaleMap DisplayScales(
        const Identifier& account) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::MessagableList& MessagableList(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::PayableList& PayableList(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::Profile& Profile(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    OPENTXS_EXPORT virtual const ui::UnitList& UnitList(
        const identifier::Nym& nym,
        const SimpleCallback updateCB = {}) const noexcept = 0;

#if OT_QT
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::AccountActivityQt* AccountActivityQt(
        const identifier::Nym& nymID,
        const Identifier& accountID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::AccountListQt* AccountListQt(
        const identifier::Nym& nym,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::AccountSummaryQt* AccountSummaryQt(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::ActivitySummaryQt* ActivitySummaryQt(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::ActivityThreadQt* ActivityThreadQt(
        const identifier::Nym& nymID,
        const Identifier& threadID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual QAbstractItemModel* BlankModel(
        const std::size_t columns) const noexcept = 0;
    /// Caller does not own this pointer
#if OT_BLOCKCHAIN
    OPENTXS_EXPORT virtual ui::BlockchainSelectionQt* BlockchainSelectionQt()
        const noexcept = 0;
#endif  // OT_BLOCKCHAIN
    OPENTXS_EXPORT virtual ui::ContactQt* ContactQt(
        const Identifier& contactID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::ContactListQt* ContactListQt(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::DisplayScaleListQt* DisplayScalesQt(
        const Identifier& account) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::MessagableListQt* MessagableListQt(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::PayableListQt* PayableListQt(
        const identifier::Nym& nymID,
        const proto::ContactItemType currency,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::ProfileQt* ProfileQt(
        const identifier::Nym& nymID,
        const SimpleCallback updateCB = {}) const noexcept = 0;
    /// Caller does not own this pointer
    OPENTXS_EXPORT virtual ui::UnitListQt* UnitListQt(
        const identifier::Nym& nym,
        const SimpleCallback updateCB = {}) const noexcept = 0;
#endif

    virtual ~UI() = default;

protected:
    UI() noexcept = default;

private:
    UI(const UI&) = delete;
    UI(UI&&) = delete;
    UI& operator=(const UI&) = delete;
    UI& operator=(UI&&) = delete;
};
}  // namespace client
}  // namespace api
}  // namespace opentxs
#endif
