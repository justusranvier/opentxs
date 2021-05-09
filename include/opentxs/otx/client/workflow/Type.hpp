// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_OTX_CLIENT_WORKFLOW_TYPE_HPP
#define OPENTXS_OTX_CLIENT_WORKFLOW_TYPE_HPP

#include "opentxs/otx/client/workflow/Types.hpp"  // IWYU pragma: associated

namespace opentxs
{
namespace otx
{
namespace client
{
namespace workflow
{
enum class Type : TypeEnum {
    Error = 0,
    Cheque = 1,
    Invoice = 2,
    Transfer = 3,
    Cash = 4,
};
}  // namespace workflow
}  // namespace client
}  // namespace otx
}  // namespace opentxs
#endif
