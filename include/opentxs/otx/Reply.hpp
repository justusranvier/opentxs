// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_OTX_REPLY_HPP
#define OPENTXS_OTX_REPLY_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include <memory>

#include "opentxs/Pimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/core/contract/Signable.hpp"
#include "opentxs/otx/Types.hpp"

namespace opentxs
{
namespace api
{
namespace internal
{
struct Core;
}  // namespace internal
}  // namespace api

namespace otx
{
class Reply;
}  // namespace otx

namespace proto
{
class OTXPush;
class ServerReply;
}  // namespace proto

using OTXReply = Pimpl<otx::Reply>;
}  // namespace opentxs

namespace opentxs
{
namespace otx
{
class OPENTXS_EXPORT Reply : virtual public opentxs::contract::Signable
{
public:
    static const VersionNumber DefaultVersion;
    static const VersionNumber MaxVersion;

    OPENTXS_NO_EXPORT static Pimpl<opentxs::otx::Reply> Factory(
        const api::internal::Core& api,
        const Nym_p signer,
        const identifier::Nym& recipient,
        const identifier::Server& server,
        const otx::ServerReplyType type,
        const RequestNumber number,
        const bool success,
        const PasswordPrompt& reason,
        std::shared_ptr<const proto::OTXPush>&& push = {});
    static Pimpl<opentxs::otx::Reply> Factory(
        const api::internal::Core& api,
        const Nym_p signer,
        const identifier::Nym& recipient,
        const identifier::Server& server,
        const otx::ServerReplyType type,
        const RequestNumber number,
        const bool success,
        const PasswordPrompt& reason,
        opentxs::otx::OTXPushType pushtype,
        const std::string& payload);
    OPENTXS_NO_EXPORT static Pimpl<opentxs::otx::Reply> Factory(
        const api::internal::Core& api,
        const proto::ServerReply serialized);
    static Pimpl<opentxs::otx::Reply> Factory(
        const api::internal::Core& api,
        const ReadView& view);

    virtual RequestNumber Number() const = 0;
    OPENTXS_NO_EXPORT virtual std::shared_ptr<const proto::OTXPush> Push()
        const = 0;
    virtual const identifier::Nym& Recipient() const = 0;
    using Signable::Serialize;
    virtual bool Serialize(AllocateOutput destination) const = 0;
    OPENTXS_NO_EXPORT virtual bool Serialize(
        proto::ServerReply& serialized) const = 0;
    virtual const identifier::Server& Server() const = 0;
    virtual bool Success() const = 0;
    virtual otx::ServerReplyType Type() const = 0;

    ~Reply() override = default;

protected:
    Reply() = default;

private:
    friend OTXReply;

#ifndef _WIN32
    Reply* clone() const noexcept override = 0;
#endif

    Reply(const Reply&) = delete;
    Reply(Reply&&) = delete;
    Reply& operator=(const Reply&) = delete;
    Reply& operator=(Reply&&) = delete;
};
}  // namespace otx
}  // namespace opentxs
#endif
