// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "rpc/RPC.tpp"     // IWYU pragma: associated

#include <string>
#include <vector>

#include "internal/api/client/Client.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/SharedPimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/UI.hpp"
#include "opentxs/api/client/Workflow.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/protobuf/PaymentWorkflowEnums.pb.h"
#include "opentxs/rpc/AccountEventType.hpp"
#include "opentxs/rpc/ResponseCode.hpp"
#include "opentxs/rpc/request/Base.hpp"
#include "opentxs/rpc/request/GetAccountActivity.hpp"
#include "opentxs/rpc/response/Base.hpp"
#include "opentxs/rpc/response/GetAccountActivity.hpp"
#include "opentxs/ui/AccountActivity.hpp"
#include "opentxs/ui/BalanceItem.hpp"
#include "rpc/RPC.hpp"

namespace opentxs::rpc::implementation
{
auto RPC::get_account_activity(const request::Base& base) const
    -> response::Base
{
    const auto& in = base.asGetAccountActivity();
    auto codes = response::Base::Responses{};
    auto events = response::GetAccountActivity::Events{};
    const auto reply = [&] {
        return response::GetAccountActivity{
            in, std::move(codes), std::move(events)};
    };

    try {
        const auto& api = client_session(base);

        for (const auto& id : in.Accounts()) {
            const auto index = codes.size();

            if (id.empty()) {
                codes.emplace_back(index, ResponseCode::invalid);

                continue;
            }

            const auto accountID = api.Factory().Identifier(id);
            const auto owner = api.Storage().AccountOwner(accountID);
            const auto& widget = api.UI().AccountActivity(owner, accountID);
            const auto copy = [&](const auto& row) {
                const auto contact = [&]() -> std::string {
                    const auto& contacts = row.Contacts();

                    if (0u < contacts.size()) {
                        return contacts.front();
                    } else if (StorageBox::INTERNALTRANSFER == row.Type()) {

                        return api.Contacts().ContactID(owner)->str();
                    }

                    return {};
                }();
                const auto state = [&] {
                    const auto workflowID =
                        api.Factory().Identifier(row.Workflow());
                    const auto workflow =
                        api.Workflow().LoadWorkflow(owner, workflowID);

                    if (workflow) { return workflow->state(); }

                    return proto::PAYMENTWORKFLOWSTATE_ERROR;
                }();
                events.emplace_back(
                    id,
                    get_account_event_type(row.Type(), row.Amount()),
                    contact,
                    row.Workflow(),
                    row.Amount(),
                    row.Amount(),
                    row.Timestamp(),
                    row.Memo(),
                    row.UUID(),
                    state);
            };
            auto row = widget.First();

            if (false == row->Valid()) {
                codes.emplace_back(index, ResponseCode::none);

                continue;
            }

            copy(row.get());

            while (false == row->Last()) {
                row = widget.Next();
                copy(row.get());
            }

            codes.emplace_back(index, ResponseCode::success);
        }
    } catch (...) {
        codes.emplace_back(0, ResponseCode::bad_session);
    }

    return reply();
}

auto RPC::get_account_event_type(StorageBox storagebox, Amount amount) noexcept
    -> rpc::AccountEventType
{
    switch (storagebox) {
        case StorageBox::INCOMINGCHEQUE: {

            return AccountEventType::incoming_cheque;
        }
        case StorageBox::OUTGOINGCHEQUE: {

            return AccountEventType::outgoing_cheque;
        }
        case StorageBox::INCOMINGTRANSFER: {

            return AccountEventType::incoming_transfer;
        }
        case StorageBox::OUTGOINGTRANSFER: {

            return AccountEventType::outgoing_transfer;
        }
        case StorageBox::INTERNALTRANSFER: {
            if (0 > amount) {

                return AccountEventType::outgoing_transfer;
            } else {

                return AccountEventType::incoming_transfer;
            }
        }
        default: {

            return AccountEventType::error;
        }
    }
}
}  // namespace opentxs::rpc::implementation
