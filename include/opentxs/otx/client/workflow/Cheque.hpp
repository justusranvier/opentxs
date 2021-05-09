// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_OTX_CLIENT_WORKFLOW_CHEQUE_HPP
#define OPENTXS_OTX_CLIENT_WORKFLOW_CHEQUE_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include "opentxs/otx/client/workflow/Base.hpp"

namespace opentxs
{
namespace proto
{
class PaymentWorkflow;
}  // namespace proto
}  // namespace opentxs

namespace opentxs
{
namespace otx
{
namespace client
{
namespace workflow
{
class OPENTXS_EXPORT Cheque final : public Base
{
public:
    OPENTXS_NO_EXPORT Cheque(const proto::PaymentWorkflow& serialized) noexcept(
        false);
    Cheque() noexcept;

    ~Cheque() final;

private:
    Cheque(const Cheque&) = delete;
    Cheque(Cheque&&) = delete;
    auto operator=(const Cheque&) -> Cheque& = delete;
    auto operator=(Cheque&&) -> Cheque& = delete;
};
}  // namespace workflow
}  // namespace client
}  // namespace otx
}  // namespace opentxs
#endif
