// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "display/Scale.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/display/Types.hpp"

namespace opentxs::display
{
class OPENTXS_EXPORT Definition
{
public:
    using NamedScale = std::pair<Name, Scale>;
    using Scales = std::vector<NamedScale>;

    auto Format(
        const Amount amount,
        const Index scale = 0,
        const OptionalInt minDecimals = std::nullopt,
        const OptionalInt maxDecimals = std::nullopt) const noexcept(false)
        -> std::string;
    auto GetScales() const noexcept -> ScaleMap;
    auto Import(const std::string& formatted, const Index scale = 0) const
        noexcept(false) -> Amount;

    Definition(Scales&& scales) noexcept;
    Definition() noexcept;
    Definition(const Definition&) noexcept;
    Definition(Definition&&) noexcept;

    auto operator=(const Definition&) noexcept -> Definition&;
    auto operator=(Definition&&) noexcept -> Definition&;

    ~Definition();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;
};
}  // namespace opentxs::display
