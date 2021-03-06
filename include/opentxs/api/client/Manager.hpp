// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_API_CLIENT_MANAGER_HPP
#define OPENTXS_API_CLIENT_MANAGER_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <string>

#include "opentxs/api/Core.hpp"

namespace opentxs
{
namespace api
{
namespace client
{
class Activity;
class Blockchain;
class Contacts;
class OTX;
class Pair;
class ServerAction;
class UI;
class Workflow;
}  // namespace client

namespace network
{
class ZMQ;
}  // namespace network
}  // namespace api

class OT_API;
class OTAPI_Exec;
}  // namespace opentxs

namespace opentxs
{
namespace api
{
namespace client
{
class OPENTXS_EXPORT Manager : virtual public api::Core
{
public:
    virtual const api::client::Activity& Activity() const = 0;
    virtual const api::client::Blockchain& Blockchain() const = 0;
    virtual const api::client::Contacts& Contacts() const = 0;
    virtual const OTAPI_Exec& Exec(const std::string& wallet = "") const = 0;
    virtual std::recursive_mutex& Lock(
        const identifier::Nym& nymID,
        const identifier::Server& serverID) const = 0;
    virtual const OT_API& OTAPI(const std::string& wallet = "") const = 0;
    virtual const client::OTX& OTX() const = 0;
    virtual const client::Pair& Pair() const = 0;
    virtual const client::ServerAction& ServerAction() const = 0;
    virtual const api::client::UI& UI() const = 0;
    virtual const client::Workflow& Workflow() const = 0;
    virtual const network::ZMQ& ZMQ() const = 0;

    OPENTXS_NO_EXPORT ~Manager() override = default;

protected:
    Manager() = default;

private:
    Manager(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager& operator=(Manager&&) = delete;
};
}  // namespace client
}  // namespace api
}  // namespace opentxs
#endif
