// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_PROTOBUF_CREDENTIAL_HPP
#define OPENTXS_PROTOBUF_CREDENTIAL_HPP

#include "opentxs/Version.hpp"  // IWYU pragma: associated

#include "opentxs/protobuf/Enums.pb.h"

namespace opentxs
{
namespace proto
{
class Credential;
}  // namespace proto
}  // namespace opentxs

namespace opentxs
{
namespace proto
{
OPENTXS_EXPORT bool CheckProto_1(
    const Credential& serializedCred,
    const bool silent,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_2(
    const Credential& serializedCred,
    const bool silent,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_3(
    const Credential& serializedCred,
    const bool silent,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_4(
    const Credential& serializedCred,
    const bool silent,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_5(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_6(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_7(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_8(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_9(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_10(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_11(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_12(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_13(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_14(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_15(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_16(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_17(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_18(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_19(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
OPENTXS_EXPORT bool CheckProto_20(
    const Credential&,
    const bool,
    const KeyMode& mode = KEYMODE_ERROR,
    const CredentialRole role = CREDROLE_ERROR,
    const bool withSigs = true);
}  // namespace proto
}  // namespace opentxs

#endif  // OPENTXS_PROTOBUF_CREDENTIAL_HPP
