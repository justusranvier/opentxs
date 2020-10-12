// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "opentxs/Types.hpp"
#include "opentxs/display/Types.hpp"

namespace opentxs::display
{
class OPENTXS_EXPORT Scale
{
public:
    /// A ratio should express the quantity of smallest values (from Amount)
    /// which represent a scale unit.
    using Ratio = std::pair<std::uint8_t, std::int8_t>;

    auto Format(
        const Amount amount,
        const OptionalInt minDecimals = std::nullopt,
        const OptionalInt maxDecimals = std::nullopt) const noexcept(false)
        -> std::string;
    auto Import(const std::string& formatted) const noexcept(false) -> Amount;
    auto Prefix() const noexcept -> std::string;
    auto Suffix() const noexcept -> std::string;

    Scale(
        const std::string& prefix,
        const std::string& suffix,
        const std::vector<Ratio>& ratios,
        const OptionalInt defaultMinDecimals = std::nullopt,
        const OptionalInt defaultMaxDecimals = std::nullopt) noexcept;
    Scale(const Scale&) noexcept;
    Scale(Scale&&) noexcept;

    ~Scale();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;

    auto operator=(const Scale&) -> Scale& = delete;
    auto operator=(Scale &&) -> Scale& = delete;
};
}  // namespace opentxs::display
