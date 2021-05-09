// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                           // IWYU pragma: associated
#include "1_Internal.hpp"                         // IWYU pragma: associated
#include "opentxs/otx/client/workflow/Types.hpp"  // IWYU pragma: associated

#include <boost/container/flat_map.hpp>

#include "opentxs/otx/client/workflow/Direction.hpp"
#include "opentxs/otx/client/workflow/State.hpp"
#include "opentxs/otx/client/workflow/Type.hpp"

namespace opentxs
{
auto print(otx::client::workflow::Direction value) noexcept -> std::string
{
    using Type = otx::client::workflow::Direction;
    static const auto map = boost::container::flat_map<Type, std::string>{
        {Type::Unsent, "unsent"},
        {Type::None, "none"},
        {Type::Incoming, "incoming"},
        {Type::Outgoing, "outgoing"},
        {Type::Internal, "internal"},
    };

    try {

        return map.at(value);
    } catch (...) {

        return "error";
    }
}

auto print(otx::client::workflow::State value) noexcept -> std::string
{
    using Type = otx::client::workflow::State;
    static const auto map = boost::container::flat_map<Type, std::string>{
        {Type::Unsent, "unsent"},
        {Type::Conveyed, "conveyed"},
        {Type::Canceled, "canceled"},
        {Type::Accepted, "accepted"},
        {Type::Completed, "completed"},
        {Type::Expired, "expired"},
        {Type::Initiated, "initiated"},
        {Type::Aborted, "aborted"},
        {Type::Acknowledged, "acknowledged"},
        {Type::Rejected, "rejected"},
    };

    try {

        return map.at(value);
    } catch (...) {

        return "error";
    }
}

auto print(otx::client::workflow::Type value) noexcept -> std::string
{
    using Type = otx::client::workflow::Type;
    static const auto map = boost::container::flat_map<Type, std::string>{
        {Type::Cheque, "cheque"},
        {Type::Invoice, "invoice"},
        {Type::Transfer, "transfer"},
        {Type::Cash, "cash"},
    };

    try {

        return map.at(value);
    } catch (...) {

        return "error";
    }
}
}  // namespace opentxs
