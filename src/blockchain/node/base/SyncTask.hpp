// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace opentxs::blockchain::node::base
{
class SyncTask
{
public:
    SyncTask() noexcept;

    ~SyncTask();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;

    SyncTask(const SyncTask&) = delete;
    SyncTask(SyncTask&&) = delete;
    auto operator=(const SyncTask&) -> SyncTask& = delete;
    auto operator=(SyncTask&&) -> SyncTask& = delete;
};
}  // namespace opentxs::blockchain::node::base
