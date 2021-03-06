// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_CONTACT_CONTACTITEM_HPP
#define OPENTXS_CONTACT_CONTACTITEM_HPP

#define NULL_START 0
#define NULL_END 0

// IWYU pragma: no_include "opentxs/contact/ContactItemAttribute.hpp"

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <chrono>
#include <cstdint>
#include <ctime>
#include <memory>
#include <set>
#include <string>

#include "opentxs/Bytes.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/contact/Types.hpp"
#include "opentxs/core/Identifier.hpp"

namespace opentxs
{
namespace api
{
namespace internal
{
struct Core;
}  // namespace internal
}  // namespace api

class Identifier;

namespace proto
{
class ContactItem;
}  // namespace proto
}  // namespace opentxs

namespace opentxs
{
class OPENTXS_EXPORT ContactItem
{
public:
    ContactItem(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber version,
        const VersionNumber parentVersion,
        const contact::ContactSectionName section,
        const contact::ContactItemType& type,
        const std::string& value,
        const std::set<contact::ContactItemAttribute>& attributes,
        const std::time_t start,
        const std::time_t end,
        const std::string subtype);
    ContactItem(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber version,
        const VersionNumber parentVersion,
        const Claim& claim);
    OPENTXS_NO_EXPORT ContactItem(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber parentVersion,
        const contact::ContactSectionName section,
        const proto::ContactItem& serialized);
    ContactItem(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber parentVersion,
        const contact::ContactSectionName section,
        const ReadView& serialized);
    ContactItem(const ContactItem&) noexcept;
    ContactItem(ContactItem&&) noexcept;

    bool operator==(const ContactItem& rhs) const;

    const std::time_t& End() const;
    const Identifier& ID() const;
    bool isActive() const;
    bool isLocal() const;
    bool isPrimary() const;
    const contact::ContactSectionName& Section() const;
    bool Serialize(AllocateOutput destination, const bool withID = false) const;
    OPENTXS_NO_EXPORT bool Serialize(
        proto::ContactItem& out,
        const bool withID = false) const;
    ContactItem SetActive(const bool active) const;
    ContactItem SetEnd(const std::time_t end) const;
    ContactItem SetLocal(const bool local) const;
    ContactItem SetPrimary(const bool primary) const;
    ContactItem SetStart(const std::time_t start) const;
    ContactItem SetValue(const std::string& value) const;
    const std::time_t& Start() const;
    const std::string& Subtype() const;
    const contact::ContactItemType& Type() const;
    const std::string& Value() const;
    VersionNumber Version() const;

    ~ContactItem();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;

    ContactItem() = delete;
    ContactItem& operator=(const ContactItem&) = delete;
    ContactItem& operator=(ContactItem&&) = delete;
};
}  // namespace opentxs

#endif
