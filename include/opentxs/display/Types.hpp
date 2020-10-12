// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_DISPLAY_TYPES_HPP
#define OPENTXS_DISPLAY_TYPES_HPP

#include "opentxs/Forward.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace opentxs
{
namespace display
{
using Index = unsigned int;
using Name = std::string;
using ScaleMap = std::map<Index, Name>;
using OptionalInt = std::optional<std::uint8_t>;
}  // namespace display
}  // namespace opentxs
#endif
