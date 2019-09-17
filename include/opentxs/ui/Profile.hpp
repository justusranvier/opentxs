// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_UI_PROFILE_HPP
#define OPENTXS_UI_PROFILE_HPP

#ifndef Q_MOC_RUN

#include "opentxs/Forward.hpp"

#include "opentxs/ui/List.hpp"
#include "opentxs/Proto.hpp"

#include <string>
#include <tuple>
#include <vector>

#ifdef SWIG
#include <algorithm>
#include <tuple>
#include <vector>

// clang-format off
%extend opentxs::ui::Profile {
    bool AddClaim(
        const int section,
        const int type,
        const std::string& value,
        const bool primary,
        const bool active) const noexcept
    {
        return $self->AddClaim(
            static_cast<opentxs::proto::ContactSectionName>(section),
            static_cast<opentxs::proto::ContactItemType>(type),
            value,
            primary,
            active);
    }
    std::vector<std::pair<int, std::string>> AllowedItems(
        const int section,
        const std::string& lang) noexcept
    {
        const auto types = opentxs::ui::ProfileSection::AllowedItems(
            static_cast<opentxs::proto::ContactSectionName>(section),
            lang);
        std::vector<std::pair<int, std::string>> output;
        std::transform(
            types.begin(),
            types.end(),
            std::inserter(output, output.end()),
            [](std::pair<opentxs::proto::ContactItemType, std::string> type) ->
                std::pair<int, std::string> {
                    return {static_cast<int>(type.first), type.second};} );

        return output;
    }
    std::vector<std::pair<int, std::string>> AllowedSections(
        const std::string& lang) noexcept
    {
        const auto sections = $self->AllowedSections(lang);
        std::vector<std::pair<int, std::string>> output;
        std::transform(
            sections.begin(),
            sections.end(),
            std::inserter(output, output.end()),
            [](std::pair<opentxs::proto::ContactSectionName, std::string> type) ->
                std::pair<int, std::string> {
                    return {static_cast<int>(type.first), type.second};} );

        return output;
    }
}
%ignore opentxs::ui::Profile::AddClaim;
%ignore opentxs::ui::Profile::AllowedItems;
%ignore opentxs::ui::Profile::AllowedSections;
%rename(UIProfile) opentxs::ui::Profile;
// clang-format on
#endif  // SWIG

namespace opentxs
{
namespace ui
{
namespace implementation
{
class Profile;
}  // namespace implementation

class Profile : virtual public List
{
public:
    using ItemType = std::pair<proto::ContactItemType, std::string>;
    using ItemTypeList = std::vector<ItemType>;
    using SectionType = std::pair<proto::ContactSectionName, std::string>;
    using SectionTypeList = std::vector<SectionType>;

    EXPORT virtual bool AddClaim(
        const proto::ContactSectionName section,
        const proto::ContactItemType type,
        const std::string& value,
        const bool primary,
        const bool active) const noexcept = 0;
    EXPORT virtual ItemTypeList AllowedItems(
        const proto::ContactSectionName section,
        const std::string& lang) const noexcept = 0;
    EXPORT virtual SectionTypeList AllowedSections(
        const std::string& lang) const noexcept = 0;
    EXPORT virtual bool Delete(
        const int section,
        const int type,
        const std::string& claimID) const noexcept = 0;
    EXPORT virtual std::string DisplayName() const noexcept = 0;
    EXPORT virtual opentxs::SharedPimpl<opentxs::ui::ProfileSection> First()
        const noexcept = 0;
    EXPORT virtual std::string ID() const noexcept = 0;
    EXPORT virtual opentxs::SharedPimpl<opentxs::ui::ProfileSection> Next()
        const noexcept = 0;
    EXPORT virtual std::string PaymentCode() const noexcept = 0;
    EXPORT virtual bool SetActive(
        const int section,
        const int type,
        const std::string& claimID,
        const bool active) const noexcept = 0;
    EXPORT virtual bool SetPrimary(
        const int section,
        const int type,
        const std::string& claimID,
        const bool primary) const noexcept = 0;
    EXPORT virtual bool SetValue(
        const int section,
        const int type,
        const std::string& claimID,
        const std::string& value) const noexcept = 0;

    EXPORT virtual ~Profile() = default;

protected:
    Profile() noexcept = default;

private:
    Profile(const Profile&) = delete;
    Profile(Profile&&) = delete;
    Profile& operator=(const Profile&) = delete;
    Profile& operator=(Profile&&) = delete;
};
}  // namespace ui
}  // namespace opentxs
#endif

#if OT_QT || defined(Q_MOC_RUN)
class opentxs::ui::ProfileQt final : public QAbstractItemModel
{
    Q_OBJECT

public:
    using ConstructorCallback = std::function<
        implementation::Profile*(RowCallbacks insert, RowCallbacks remove)>;

    enum Roles {};

    QString displayName() const noexcept;
    QString nymID() const noexcept;
    QString paymentCode() const noexcept;

    int columnCount(const QModelIndex& parent = QModelIndex()) const
        noexcept final;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const
        noexcept final;
    QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const noexcept final;
    QModelIndex parent(const QModelIndex& index) const noexcept final;
    QHash<int, QByteArray> roleNames() const noexcept final;
    int rowCount(const QModelIndex& parent = QModelIndex()) const
        noexcept final;

    const Profile& operator*() const noexcept;

    // Throws std::runtime_error if callback returns invalid pointer
    ProfileQt(ConstructorCallback cb) noexcept(false);
    ~ProfileQt() final;

signals:
    void updated() const;

private:
    Q_PROPERTY(QString displayName READ displayName NOTIFY updated)
    Q_PROPERTY(QString nymID READ nymID NOTIFY updated)
    Q_PROPERTY(QString paymentCode READ paymentCode NOTIFY updated)

    std::unique_ptr<implementation::Profile> parent_;

    void notify() const noexcept;
    void finish_row_add() noexcept;
    void finish_row_delete() noexcept;
    void start_row_add(const QModelIndex& parent, int first, int last) noexcept;
    void start_row_delete(
        const QModelIndex& parent,
        int first,
        int last) noexcept;

    ProfileQt() = delete;
    ProfileQt(const ProfileQt&) = delete;
    ProfileQt(ProfileQt&&) = delete;
    ProfileQt& operator=(const ProfileQt&) = delete;
    ProfileQt& operator=(ProfileQt&&) = delete;
};
#endif
#endif
