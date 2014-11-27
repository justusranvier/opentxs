/************************************************************
 *
 *  OTPayment.hpp
 *
 */

/************************************************************
 -----BEGIN PGP SIGNED MESSAGE-----
 Hash: SHA1

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
 *  Copyright (C) 2010-2013 by "Fellow Traveler" (A pseudonym)
 *
 *  EMAIL:
 *  FellowTraveler@rayservers.net
 *
 *  BITCOIN:  1NtTPVVjDsUfDWybS4BwvHpG2pdS9RnYyQ
 *
 *  KEY FINGERPRINT (PGP Key in license file):
 *  9DD5 90EB 9292 4B48 0484  7910 0308 00ED F951 BB8E
 *
 *  OFFICIAL PROJECT WIKI(s):
 *  https://github.com/FellowTraveler/Moneychanger
 *  https://github.com/FellowTraveler/Open-Transactions/wiki
 *
 *  WEBSITE:
 *  http://www.OpenTransactions.org/
 *
 *  Components and licensing:
 *   -- Moneychanger..A Java client GUI.....LICENSE:.....GPLv3
 *   -- otlib.........A class library.......LICENSE:...LAGPLv3
 *   -- otapi.........A client API..........LICENSE:...LAGPLv3
 *   -- opentxs/ot....Command-line client...LICENSE:...LAGPLv3
 *   -- otserver......Server Application....LICENSE:....AGPLv3
 *  Github.com/FellowTraveler/Open-Transactions/wiki/Components
 *
 *  All of the above OT components were designed and written by
 *  Fellow Traveler, with the exception of Moneychanger, which
 *  was contracted out to Vicky C (bitcointrader4@gmail.com).
 *  The open-source community has since actively contributed.
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This program is free software: you can redistribute it
 *   and/or modify it under the terms of the GNU Affero
 *   General Public License as published by the Free Software
 *   Foundation, either version 3 of the License, or (at your
 *   option) any later version.
 *
 *   ADDITIONAL PERMISSION under the GNU Affero GPL version 3
 *   section 7: (This paragraph applies only to the LAGPLv3
 *   components listed above.) If you modify this Program, or
 *   any covered work, by linking or combining it with other
 *   code, such other code is not for that reason alone subject
 *   to any of the requirements of the GNU Affero GPL version 3.
 *   (==> This means if you are only using the OT API, then you
 *   don't have to open-source your code--only your changes to
 *   Open-Transactions itself must be open source. Similar to
 *   LGPLv3, except it applies to software-as-a-service, not
 *   just to distributing binaries.)
 *
 *   Extra WAIVER for OpenSSL, Lucre, and all other libraries
 *   used by Open Transactions: This program is released under
 *   the AGPL with the additional exemption that compiling,
 *   linking, and/or using OpenSSL is allowed. The same is true
 *   for any other open source libraries included in this
 *   project: complete waiver from the AGPL is hereby granted to
 *   compile, link, and/or use them with Open-Transactions,
 *   according to their own terms, as long as the rest of the
 *   Open-Transactions terms remain respected, with regard to
 *   the Open-Transactions code itself.
 *
 *   Lucre License:
 *   This code is also "dual-license", meaning that Ben Lau-
 *   rie's license must also be included and respected, since
 *   the code for Lucre is also included with Open Transactions.
 *   See Open-Transactions/src/otlib/lucre/LUCRE_LICENSE.txt
 *   The Laurie requirements are light, but if there is any
 *   problem with his license, simply remove the Lucre code.
 *   Although there are no other blind token algorithms in Open
 *   Transactions (yet. credlib is coming), the other functions
 *   will continue to operate.
 *   See Lucre on Github:  https://github.com/benlaurie/lucre
 *   -----------------------------------------------------
 *   You should have received a copy of the GNU Affero General
 *   Public License along with this program.  If not, see:
 *   http://www.gnu.org/licenses/
 *
 *   If you would like to use this software outside of the free
 *   software license, please contact FellowTraveler.
 *   (Unfortunately many will run anonymously and untraceably,
 *   so who could really stop them?)
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will be
 *   useful, but WITHOUT ANY WARRANTY; without even the implied
 *   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *   PURPOSE.  See the GNU Affero General Public License for
 *   more details.

 -----BEGIN PGP SIGNATURE-----
 Version: GnuPG v1.4.9 (Darwin)

 iQIcBAEBAgAGBQJRSsfJAAoJEAMIAO35UbuOQT8P/RJbka8etf7wbxdHQNAY+2cC
 vDf8J3X8VI+pwMqv6wgTVy17venMZJa4I4ikXD/MRyWV1XbTG0mBXk/7AZk7Rexk
 KTvL/U1kWiez6+8XXLye+k2JNM6v7eej8xMrqEcO0ZArh/DsLoIn1y8p8qjBI7+m
 aE7lhstDiD0z8mwRRLKFLN2IH5rAFaZZUvj5ERJaoYUKdn4c+RcQVei2YOl4T0FU
 LWND3YLoH8naqJXkaOKEN4UfJINCwxhe5Ke9wyfLWLUO7NamRkWD2T7CJ0xocnD1
 sjAzlVGNgaFDRflfIF4QhBx1Ddl6wwhJfw+d08bjqblSq8aXDkmFA7HeunSFKkdn
 oIEOEgyj+veuOMRJC5pnBJ9vV+7qRdDKQWaCKotynt4sWJDGQ9kWGWm74SsNaduN
 TPMyr9kNmGsfR69Q2Zq/FLcLX/j8ESxU+HYUB4vaARw2xEOu2xwDDv6jt0j3Vqsg
 x7rWv4S/Eh18FDNDkVRChiNoOIilLYLL6c38uMf1pnItBuxP3uhgY6COm59kVaRh
 nyGTYCDYD2TK+fI9o89F1297uDCwEJ62U0Q7iTDp5QuXCoxkPfv8/kX6lS6T3y9G
 M9mqIoLbIQ1EDntFv7/t6fUTS2+46uCrdZWbQ5RjYXdrzjij02nDmJAm2BngnZvd
 kamH0Y/n11lCvo1oQxM+
 =uSzz
 -----END PGP SIGNATURE-----
 **************************************************************/

#ifndef OPENTXS_EXT_OTPAYMENT_HPP
#define OPENTXS_EXT_OTPAYMENT_HPP

#include <opentxs/core/Contract.hpp>

namespace opentxs
{

class Cheque;
class NumList;
class OTPaymentPlan;
class Purse;
class OTSmartContract;
class OTTrackable;

/*
  The PAYMENT can be of types:
    - CHEQUE, INVOICE, VOUCHER (these are all forms of cheque)
    - PAYMENT PLAN, SMART CONTRACT (these are cron items)
    - PURSE (containing cash)

 FYI:

 OTContract — Most other classes are derived from this one. Contains the actual
 XML contents,
  as well as various data values that were loaded from those contents, including
 public keys.
  Also contains a list of signatures.

 OTScriptable — Derived from OTContract, but enables scriptable clauses. Also
 contains a list
  of parties (each with agents and asset accounts) as well as a list of bylaws
 (each with scripted
  clauses, internal state, hooks, callbacks, etc.)

 OTInstrument — Has a date range, a server ID, and an instrument definition id.
 Derived from
 OTScriptable.

 OTTrackable  — Has a transaction number, user ID, and an asset account ID.
 Derived from OTInstrument.

 OTCheque — A financial instrument. Derived from OTTrackable.

 OTCronItem — Derived from OTTrackable. OT has a central “Cron” object which
 runs recurring tasks, known as CronItems.

 OTAgreement — Derived from OTCronItem. It has a recipient and recipient asset
 account.

 OTPaymentPlan — Derived from OTAgreement, derived from OTCronItem. Allows
 merchants and customers
  to set up recurring payments. (Cancel anytime, with a receipt going to both
 inboxes.)

 OTSmartContract — Derived from OTCronItem. All CronItems are actually derived
 from OTScriptable already
  (through OTTrackable/OTInstrument). But OTSmartContract is the first/only Cron
 Item specifically designed
  to take full advantage of both the cron system AND the scriptable system in
 conjunction with each other.
  Currently OTSmartContract is the only actual server-side scripting on OT.
 */

class OTPayment : public Contract
{
private: // Private prevents erroneous use by other classes.
    typedef Contract ot_super;

public:
    enum paymentType {
        // OTCheque is derived from OTTrackable, which is derived from
        // OTInstrument, which is
        // derived from OTScriptable, which is derived from OTContract.
        CHEQUE,  // A cheque drawn on a user's account.
        VOUCHER, // A cheque drawn on a server account (cashier's cheque aka
                 // banker's cheque)
        INVOICE, // A cheque with a negative amount. (Depositing this causes a
                 // payment out, instead of a deposit in.)
        PAYMENT_PLAN,   // An OTCronItem-derived OTPaymentPlan, related to a
                        // recurring payment plan.
        SMART_CONTRACT, // An OTCronItem-derived OTSmartContract, related to a
                        // smart contract.
        PURSE, // An OTContract-derived OTPurse containing a list of cash
               // OTTokens.
        ERROR_STATE
    }; // If you add any types to this list, update the list of strings at the
       // top of the .CPP file.

protected:
    virtual void UpdateContents(); // Before transmission or serialization, this
                                   // is where the object saves its contents
    String m_strPayment; // Contains the cheque / payment plan / etc in string
                         // form.
    paymentType m_Type;  // Default value is ERROR_STATE
    // Once the actual instrument is loaded up, we copy some temp values to
    // *this
    // object. Until then, this bool (m_bAreTempValuesSet) is set to false.
    //
    bool m_bAreTempValuesSet;

    // Here are the TEMP values:
    // (These are not serialized.)
    //
    bool m_bHasRecipient; // For cheques mostly, and payment plans too.
    bool m_bHasRemitter; // For vouchers (cashier's cheques), the Nym who bought
                         // the voucher is the remitter, whereas the "sender" is
                         // the server Nym whose account the voucher is drawn
                         // on.

    int64_t m_lAmount; // Contains 0 by default. This is set by SetPayment()
                       // along with other useful values.
    int64_t m_lTransactionNum; // Contains 0 by default. This is set by
                               // SetPayment() along with other useful values.

    String m_strMemo; // Memo, Consideration, Subject, etc.

    Identifier m_InstrumentDefinitionID; // These are for convenience only, for
                                         // caching
                                         // once they happen to be loaded.
    Identifier m_NotaryID;     // These values are NOT serialized other than via
                               // the payment instrument itself
    Identifier m_SenderNymID;  // (where they are captured from, whenever it
                               // is instantiated.) Until m_bAreTempValuesSet
    Identifier m_SenderAcctID; // is set to true, these values can NOT be
                               // considered available. Use the accessing
                               // methods
    Identifier m_RecipientNymID;  // below. These values are not ALL always
                                  // available, depending on the payment
                                  // instrument
    Identifier m_RecipientAcctID; // type. Different payment instruments
                                  // support different temp values.
    Identifier m_RemitterNymID;   // A voucher (cashier's cheque) has the
                                  // "bank" as the sender. Whereas the Nym who
                                  // actually purchased the voucher is the
                                  // remitter.
    Identifier m_RemitterAcctID;  // A voucher (cashier's cheque) has the
                                  // "bank"s account as the sender acct.
                                  // Whereas the account that was originally
                                  // used to purchase the voucher is the
                                  // remitter account.
    time64_t m_VALID_FROM;        // Temporary values. Not always available.
    time64_t m_VALID_TO;          // Temporary values. Not always available.
public:
    EXPORT bool SetPayment(const String& strPayment);

    EXPORT bool IsCheque() const
    {
        return (CHEQUE == m_Type);
    }
    EXPORT bool IsVoucher() const
    {
        return (VOUCHER == m_Type);
    }
    EXPORT bool IsInvoice() const
    {
        return (INVOICE == m_Type);
    }
    EXPORT bool IsPaymentPlan() const
    {
        return (PAYMENT_PLAN == m_Type);
    }
    EXPORT bool IsSmartContract() const
    {
        return (SMART_CONTRACT == m_Type);
    }
    EXPORT bool IsPurse() const
    {
        return (PURSE == m_Type);
    }
    EXPORT bool IsValid() const
    {
        return (ERROR_STATE != m_Type);
    }

    EXPORT paymentType GetType() const
    {
        return m_Type;
    }
    EXPORT OTTrackable* Instantiate() const;
    EXPORT OTTrackable* Instantiate(const String& strPayment);
    EXPORT Purse* InstantiatePurse() const;
    //        OTPurse * InstantiatePurse(const OTIdentifier& NOTARY_ID) const;
    //        OTPurse * InstantiatePurse(const OTIdentifier& NOTARY_ID, const
    // OTIdentifier& INSTRUMENT_DEFINITION_ID) const;

    EXPORT Purse* InstantiatePurse(const String& strPayment);
    //        OTPurse * InstantiatePurse(const OTIdentifier& NOTARY_ID,
    //                                   const OTString& strPayment);
    //        OTPurse * InstantiatePurse(const OTIdentifier& NOTARY_ID, const
    // OTIdentifier& INSTRUMENT_DEFINITION_ID,
    //                                   const OTString& strPayment);
    EXPORT bool GetPaymentContents(String& strOutput) const
    {
        strOutput = m_strPayment;
        return true;
    }

    // Since the temp values are not available until at least ONE instantiating
    // has occured,
    // this function forces that very scenario (cleanly) so you don't have to
    // instantiate-and-
    // then-delete a payment instrument. Instead, just call this, and then the
    // temp values will
    // be available thereafter.
    //
    EXPORT bool SetTempValues();

    EXPORT bool SetTempValuesFromCheque(const Cheque& theInput);
    EXPORT bool SetTempValuesFromPaymentPlan(const OTPaymentPlan& theInput);
    EXPORT bool SetTempValuesFromSmartContract(const OTSmartContract& theInput);
    EXPORT bool SetTempValuesFromPurse(const Purse& theInput);
    // Once you "Instantiate" the first time, then these values are
    // set, if available, and can be queried thereafter from *this.
    // Otherwise, these functions will return false.
    //
    EXPORT bool GetAmount(int64_t& lOutput) const;
    EXPORT bool GetTransactionNum(int64_t& lOutput) const;
    // Only works for payment plans and smart contracts. Gets the
    // opening transaction number for a given Nym, if applicable.
    // (Or closing number for a given asset account.)
    EXPORT bool GetOpeningNum(int64_t& lOutput,
                              const Identifier& theNymID) const;
    EXPORT bool GetClosingNum(int64_t& lOutput,
                              const Identifier& theAcctID) const;
    EXPORT bool GetAllTransactionNumbers(NumList& numlistOutput) const;
    EXPORT bool HasTransactionNum(const int64_t& lInput) const;
    EXPORT bool GetMemo(String& strOutput) const;
    EXPORT bool GetInstrumentDefinitionID(Identifier& theOutput) const;
    EXPORT bool GetNotaryID(Identifier& theOutput) const;
    EXPORT bool GetSenderNymID(Identifier& theOutput) const;
    EXPORT bool GetSenderAcctID(Identifier& theOutput) const;
    EXPORT bool GetRecipientNymID(Identifier& theOutput) const;
    EXPORT bool GetRecipientAcctID(Identifier& theOutput) const;
    EXPORT bool GetRemitterNymID(Identifier& theOutput) const;
    EXPORT bool GetRemitterAcctID(Identifier& theOutput) const;
    EXPORT bool GetSenderNymIDForDisplay(Identifier& theOutput) const;
    EXPORT bool GetSenderAcctIDForDisplay(Identifier& theOutput) const;
    EXPORT bool GetValidFrom(time64_t& tOutput) const;
    EXPORT bool GetValidTo(time64_t& tOutput) const;
    EXPORT bool VerifyCurrentDate(bool& bVerified); // Verify whether the
                                                    // CURRENT date is WITHIN
                                                    // the VALID FROM / TO
                                                    // dates.
    EXPORT bool IsExpired(bool& bExpired); // Verify whether the CURRENT date is
                                           // AFTER the the "VALID TO" date.
    EXPORT OTPayment();
    EXPORT OTPayment(const String& strPayment);
    EXPORT virtual ~OTPayment();
    EXPORT void InitPayment();
    EXPORT virtual void Release();
    EXPORT void Release_Payment();

    EXPORT virtual int32_t ProcessXMLNode(irr::io::IrrXMLReader*& xml);
    EXPORT static const char* _GetTypeString(paymentType theType);
    EXPORT const char* GetTypeString() const
    {
        return _GetTypeString(m_Type);
    }
    EXPORT static paymentType GetTypeFromString(const String& strType);
};

} // namespace opentxs

#endif // OPENTXS_EXT_OTPAYMENT_HPP
