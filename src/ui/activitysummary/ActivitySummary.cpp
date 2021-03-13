// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                            // IWYU pragma: associated
#include "1_Internal.hpp"                          // IWYU pragma: associated
#include "ui/activitysummary/ActivitySummary.hpp"  // IWYU pragma: associated

#include <algorithm>
#include <chrono>
#include <list>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <thread>
#include <utility>

#include "internal/api/client/Client.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/client/Activity.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/protobuf/StorageThread.pb.h"
#include "opentxs/protobuf/StorageThreadItem.pb.h"
#if OT_QT
#include "opentxs/ui/qt/ActivitySummary.hpp"
#endif  // OT_QT
#include "ui/base/List.hpp"

#define OT_METHOD "opentxs::ui::implementation::ActivitySummary::"

namespace opentxs::factory
{
auto ActivitySummaryModel(
    const api::client::internal::Manager& api,
    const Flag& running,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) noexcept
    -> std::unique_ptr<ui::implementation::ActivitySummary>
{
    using ReturnType = ui::implementation::ActivitySummary;

    return std::make_unique<ReturnType>(api, running, nymID, cb);
}

#if OT_QT
auto ActivitySummaryQtModel(
    ui::implementation::ActivitySummary& parent) noexcept
    -> std::unique_ptr<ui::ActivitySummaryQt>
{
    using ReturnType = ui::ActivitySummaryQt;

    return std::make_unique<ReturnType>(parent);
}
#endif  // OT_QT
}  // namespace opentxs::factory

#if OT_QT
namespace opentxs::ui
{
QT_PROXY_MODEL_WRAPPER(ActivitySummaryQt, implementation::ActivitySummary)
}  // namespace opentxs::ui
#endif

namespace opentxs::ui::implementation
{

ActivitySummary::ActivitySummary(
    const api::client::internal::Manager& api,
    const Flag& running,
    const identifier::Nym& nymID,
    const SimpleCallback& cb) noexcept
    : ActivitySummaryList(
          api,
          nymID,
          cb,
          true
#if OT_QT
          ,
          Roles{},
          6
#endif
          )
    , listeners_(
          {{api_.Activity().ThreadPublisher(nymID),
            new MessageProcessor<ActivitySummary>(
                &ActivitySummary::process_thread)}})
    , running_(running)
{
    init();
    setup_listeners(listeners_);
    startup_.reset(new std::thread(&ActivitySummary::startup, this));

    OT_ASSERT(startup_)
}

auto ActivitySummary::construct_row(
    const ActivitySummaryRowID& id,
    const ActivitySummarySortKey& index,
    CustomData& custom) const noexcept -> RowPointer
{
    return factory::ActivitySummaryItem(
        *this, api_, primary_id_, id, index, custom, running_);
}

auto ActivitySummary::display_name(
    const proto::StorageThread& thread) const noexcept -> std::string
{
    std::set<std::string> names{};

    for (const auto& participant : thread.participant()) {
        auto name =
            api_.Contacts().ContactName(Identifier::Factory(participant));

        if (name.empty()) {
            names.emplace(participant);
        } else {
            names.emplace(std::move(name));
        }
    }

    if (names.empty()) { return thread.id(); }

    std::stringstream stream{};

    for (const auto& name : names) { stream << name << ", "; }

    std::string output = stream.str();

    if (0 < output.size()) { output.erase(output.size() - 2, 2); }

    return output;
}

auto ActivitySummary::newest_item(
    const Identifier& id,
    const proto::StorageThread& thread,
    CustomData& custom) noexcept -> const proto::StorageThreadItem&
{
    const proto::StorageThreadItem* output{nullptr};
    auto* time = new Time;

    OT_ASSERT(nullptr != time);

    for (const auto& item : thread.item()) {
        if (nullptr == output) {
            output = &item;
            *time = Clock::from_time_t(item.time());

            continue;
        }

        if (item.time() > output->time()) {
            output = &item;
            *time = Clock::from_time_t(item.time());

            continue;
        }
    }

    OT_ASSERT(nullptr != output);

    custom.emplace_back(new std::string(output->id()));
    custom.emplace_back(new StorageBox(static_cast<StorageBox>(output->box())));
    custom.emplace_back(new std::string(output->account()));
    custom.emplace_back(time);
    custom.emplace_back(new OTIdentifier{id});

    return *output;
}

void ActivitySummary::process_thread(const std::string& id) noexcept
{
    const auto threadID = Identifier::Factory(id);
    const auto thread = api_.Activity().Thread(primary_id_, threadID);

    OT_ASSERT(thread);

    auto custom = CustomData{};
    const auto name = display_name(*thread);
    const auto time = Time(
        std::chrono::seconds(newest_item(threadID, *thread, custom).time()));
    const ActivitySummarySortKey index{time, name};
    add_item(threadID, index, custom);
}

void ActivitySummary::process_thread(const Message& message) noexcept
{
    wait_for_startup();
    const auto body = message.Body();

    OT_ASSERT(1 < body.size());

    const auto threadID = [&] {
        auto output = api_.Factory().Identifier();
        output->Assign(body.at(1).Bytes());

        return output;
    }();

    OT_ASSERT(false == threadID->empty())

    delete_item(threadID);
    process_thread(threadID->str());
}

void ActivitySummary::startup() noexcept
{
    const auto threads = api_.Activity().Threads(primary_id_, false);
    LogDetail(OT_METHOD)(__FUNCTION__)(": Loading ")(threads.size())(
        " threads.")
        .Flush();
    for (const auto& [id, alias] : threads) {
        [[maybe_unused]] const auto& notUsed = alias;
        process_thread(id);
    }

    finish_startup();
}

ActivitySummary::~ActivitySummary()
{
    for (auto& it : listeners_) { delete it.second; }
}
}  // namespace opentxs::ui::implementation
