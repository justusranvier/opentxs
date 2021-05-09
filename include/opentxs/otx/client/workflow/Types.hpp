// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_OTX_CLIENT_TYPES_HPP
#define OPENTXS_OTX_CLIENT_TYPES_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <string>

namespace opentxs
{
namespace otx
{
namespace client
{
namespace workflow
{
using TypeEnum = std::uint32_t;

enum class Direction : TypeEnum;
enum class State : TypeEnum;
enum class Type : TypeEnum;
}  // namespace workflow
}  // namespace client
}  // namespace otx

OPENTXS_EXPORT auto print(otx::client::workflow::Direction value) noexcept
    -> std::string;
OPENTXS_EXPORT auto print(otx::client::workflow::State value) noexcept
    -> std::string;
OPENTXS_EXPORT auto print(otx::client::workflow::Type value) noexcept
    -> std::string;

constexpr auto value(otx::client::workflow::Direction in) noexcept
{
    return static_cast<otx::client::workflow::TypeEnum>(in);
}
constexpr auto value(otx::client::workflow::State in) noexcept
{
    return static_cast<otx::client::workflow::TypeEnum>(in);
}
constexpr auto value(otx::client::workflow::Type in) noexcept
{
    return static_cast<otx::client::workflow::TypeEnum>(in);
}
}  // namespace opentxs
#endif
