// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                           // IWYU pragma: associated
#include "1_Internal.hpp"                         // IWYU pragma: associated
#include "blockchain/database/common/Config.hpp"  // IWYU pragma: associated

extern "C" {
#include <lmdb.h>
}

#include <utility>

#include "blockchain/database/common/Database.hpp"
#include "internal/blockchain/database/common/Common.hpp"
#include "opentxs/Bytes.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/util/WorkType.hpp"
#include "util/LMDB.hpp"

// #define OT_METHOD
// "opentxs::blockchain::database::common::Configuration::"

namespace opentxs::blockchain::database::common
{
template <typename Input>
auto tsv(const Input& in) noexcept -> ReadView
{
    return {reinterpret_cast<const char*>(&in), sizeof(in)};
}

Configuration::Configuration(
    const api::Core& api,
    opentxs::storage::lmdb::LMDB& lmdb) noexcept
    : api_(api)
    , lmdb_(lmdb)
    , config_table_(Table::ConfigMulti)
    , socket_([&] {
        auto out = api_.Network().ZeroMQ().PublishSocket();
        const auto rc =
            out->Start(api_.Endpoints().BlockchainSyncServerUpdated());

        OT_ASSERT(rc);

        return out;
    }())
{
}

auto Configuration::AddSyncServer(const std::string& endpoint) const noexcept
    -> bool
{
    if (endpoint.empty()) { return false; }

    const auto [success, code] = lmdb_.Store(
        config_table_,
        tsv(Database::Key::SyncServerEndpoint),
        endpoint,
        nullptr,
        MDB_NODUPDATA);

    if (success && (MDB_KEYEXIST != code)) {
        auto work =
            api_.Network().ZeroMQ().TaggedMessage(WorkType::SyncServerUpdated);
        work->AddFrame(endpoint);
        work->AddFrame(success);
        socket_->Send(work);

        return true;
    }

    return MDB_KEYEXIST == code;
}

auto Configuration::DeleteSyncServer(const std::string& endpoint) const noexcept
    -> bool
{
    if (endpoint.empty()) { return false; }

    const auto output = lmdb_.Delete(
        config_table_,
        tsv(Database::Key::SyncServerEndpoint),
        endpoint,
        nullptr);

    if (output) {
        static constexpr auto deleted{false};
        auto work =
            api_.Network().ZeroMQ().TaggedMessage(WorkType::SyncServerUpdated);
        work->AddFrame(endpoint);
        work->AddFrame(deleted);
        socket_->Send(work);

        return true;
    }

    return true;
}

auto Configuration::GetSyncServers() const noexcept -> Endpoints
{
    auto output = Endpoints{};
    lmdb_.Load(
        config_table_,
        tsv(Database::Key::SyncServerEndpoint),
        [&](const auto view) { output.emplace_back(view); },
        opentxs::storage::lmdb::LMDB::Mode::Multiple);

    return output;
}
}  // namespace opentxs::blockchain::database::common
