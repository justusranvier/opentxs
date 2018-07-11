/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#ifndef OPENTXS_CORE_CRYPTO_OTENVELOPE_HPP
#define OPENTXS_CORE_CRYPTO_OTENVELOPE_HPP

#include "opentxs/Forward.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>

namespace opentxs
{
typedef std::multimap<std::string, crypto::key::Asymmetric*>
    mapOfAsymmetricKeys;
typedef std::set<const Nym*> setOfNyms;

class OTEnvelope
{
public:
    EXPORT OTEnvelope();
    EXPORT explicit OTEnvelope(const OTASCIIArmor& theArmoredText);

    /** Retrieve ciphertext in ascii armored form */
    EXPORT bool GetCiphertext(OTASCIIArmor& theArmoredText) const;
    /** Load ascii armored ciphertext */
    EXPORT bool SetCiphertext(const OTASCIIArmor& theArmoredText);

    EXPORT bool Encrypt(
        const String& theInput,
        crypto::key::LegacySymmetric& theKey,
        const OTPassword& thePassword);
    EXPORT bool Decrypt(
        String& theOutput,
        const crypto::key::LegacySymmetric& theKey,
        const OTPassword& thePassword);
    EXPORT bool Seal(const setOfNyms& recipients, const String& theInput);
    EXPORT bool Seal(const Nym& theRecipient, const String& theInput);
    EXPORT bool Seal(
        const mapOfAsymmetricKeys& recipientKeys,
        const String& theInput);
    EXPORT bool Seal(
        const crypto::key::Asymmetric& RecipPubKey,
        const String& theInput);
    EXPORT bool Open(
        const Nym& theRecipient,
        String& theOutput,
        const OTPasswordData* pPWData = nullptr);

    EXPORT ~OTEnvelope() = default;

private:
    friend Letter;

    OTData ciphertext_;
};
}  // namespace opentxs
#endif  // OPENTXS_CORE_CRYPTO_OTENVELOPE_HPP
