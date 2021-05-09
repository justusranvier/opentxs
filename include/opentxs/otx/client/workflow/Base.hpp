// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IWYU pragma: no_include "opentxs/otx/client/workflow/Direction.hpp"
// IWYU pragma: no_include "opentxs/otx/client/workflow/State.hpp"
// IWYU pragma: no_include "opentxs/otx/client/workflow/Type.hpp"

#ifndef OPENTXS_OTX_CLIENT_WORKFLOW_BASE_HPP
#define OPENTXS_OTX_CLIENT_WORKFLOW_BASE_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <memory>

#include "opentxs/Bytes.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/otx/client/workflow/Types.hpp"

namespace opentxs
{
namespace otx
{
namespace client
{
namespace workflow
{
class Base;
class Cash;
class Cheque;
class Invoice;
class Transfer;
}  // namespace workflow
}  // namespace client
}  // namespace otx

namespace proto
{
class PaymentWorkflow;
}  // namespace proto

class Identifier;
}  // namespace opentxs

namespace opentxs
{
namespace otx
{
namespace client
{
namespace workflow
{
class OPENTXS_EXPORT Base
{
public:
    auto asCash() const noexcept -> const Cash&;
    auto asCheque() const noexcept -> const Cheque&;
    auto asInvoice() const noexcept -> const Invoice&;
    auto asTransfer() const noexcept -> const Transfer&;

    auto Direction() const noexcept -> workflow::Direction;
    auto ID() const noexcept -> const Identifier&;
    auto Serialize(AllocateOutput dest) const noexcept -> bool;
    OPENTXS_NO_EXPORT auto Serialize(
        proto::PaymentWorkflow& dest) const noexcept -> bool;
    auto State() const noexcept -> workflow::State;
    auto Type() const noexcept -> workflow::Type;
    auto Version() const noexcept -> VersionNumber;

    Base() noexcept;

    virtual ~Base();

protected:
    std::unique_ptr<Imp> imp_;

    Base(std::unique_ptr<Imp> imp) noexcept;

private:
    Base(const Base&) = delete;
    Base(Base&&) = delete;
    auto operator=(const Base&) -> Base& = delete;
    auto operator=(Base&&) -> Base& = delete;
};
}  // namespace workflow
}  // namespace client
}  // namespace otx
}  // namespace opentxs
#endif
