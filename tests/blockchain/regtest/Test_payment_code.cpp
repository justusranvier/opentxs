// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Helpers.hpp"  // IWYU pragma: associated

#include <gtest/gtest.h>
#include <algorithm>
#include <deque>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "OTTestEnvironment.hpp"
#include "UIHelpers.hpp"
#include "integration/Helpers.hpp"
#include "opentxs/OT.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/SharedPimpl.hpp"
#include "opentxs/api/Context.hpp"
#include "opentxs/api/HDSeed.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/api/client/Blockchain.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/client/OTX.hpp"
#include "opentxs/api/client/UI.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/FilterType.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/bitcoin/Block.hpp"
#include "opentxs/blockchain/block/bitcoin/Input.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/block/bitcoin/Inputs.hpp"
#include "opentxs/blockchain/block/bitcoin/Output.hpp"
#include "opentxs/blockchain/block/bitcoin/Outputs.hpp"
#include "opentxs/blockchain/block/bitcoin/Script.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/block/bitcoin/Transaction.hpp"
#include "opentxs/blockchain/crypto/Account.hpp"
#include "opentxs/blockchain/crypto/Element.hpp"
#include "opentxs/blockchain/crypto/HD.hpp"
#include "opentxs/blockchain/crypto/PaymentCode.hpp"
#include "opentxs/blockchain/crypto/SubaccountType.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/blockchain/node/Wallet.hpp"
#include "opentxs/contact/ContactItemType.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/crypto/PaymentCode.hpp"
#include "opentxs/core/identifier/Server.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/crypto/Language.hpp"
#include "opentxs/crypto/SeedStyle.hpp"
#include "opentxs/crypto/key/EllipticCurve.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/otx/LastReplyStatus.hpp"
#include "opentxs/rpc/ResponseCode.hpp"
#include "opentxs/rpc/request/ListAccounts.hpp"
#include "opentxs/rpc/response/Base.hpp"
#include "opentxs/rpc/response/ListAccounts.hpp"
#include "opentxs/ui/AccountActivity.hpp"
#include "opentxs/ui/BalanceItem.hpp"
#include "paymentcode/VectorsV3.hpp"

namespace opentxs
{
namespace api
{
namespace server
{
class Manager;
}  // namespace server
}  // namespace api
}  // namespace opentxs

namespace ottest
{
bool init_{false};
Server server_1_{};
Counter account_activity_alice_{};
Counter account_activity_bob_{};

namespace bca = opentxs::blockchain::crypto;

class Regtest_payment_code : public Regtest_fixture_normal
{
protected:
    using Subchain = bca::Subchain;
    using Transactions = std::deque<ot::blockchain::block::pTxid>;

    static ot::Nym_p alice_p_;
    static ot::Nym_p bob_p_;
    static Transactions transactions_;
    static std::unique_ptr<ScanListener> listener_alice_p_;
    static std::unique_ptr<ScanListener> listener_bob_p_;
    static Expected expected_;

    const ot::api::server::Manager& api_server_1_;
    const ot::identity::Nym& alice_;
    const ot::identity::Nym& bob_;
    const ot::identifier::Server& expected_notary_;
    const ot::identifier::UnitDefinition& expected_unit_;
    const std::string expected_display_unit_;
    const std::string expected_account_name_;
    const std::string expected_notary_name_;
    const std::string memo_outgoing_;
    const ot::AccountType expected_account_type_;
    const ot::contact::ContactItemType expected_unit_type_;
    const Generator mine_to_alice_;
    ScanListener& listener_alice_;
    ScanListener& listener_bob_;

    auto ReceiveHD() const noexcept -> const bca::HD&
    {
        return client_2_.Blockchain()
            .Account(bob_.ID(), test_chain_)
            .GetHD()
            .at(0);
    }
    auto ReceivePC() const noexcept -> const bca::PaymentCode&
    {
        return client_2_.Blockchain()
            .Account(bob_.ID(), test_chain_)
            .GetPaymentCode()
            .at(0);
    }
    auto SendHD() const noexcept -> const bca::HD&
    {
        return client_1_.Blockchain()
            .Account(alice_.ID(), test_chain_)
            .GetHD()
            .at(0);
    }
    auto SendPC() const noexcept -> const bca::PaymentCode&
    {
        return client_1_.Blockchain()
            .Account(alice_.ID(), test_chain_)
            .GetPaymentCode()
            .at(0);
    }

    auto Shutdown() noexcept -> void final
    {
        listener_bob_p_.reset();
        listener_alice_p_.reset();
        transactions_.clear();
        bob_p_.reset();
        alice_p_.reset();
        Regtest_fixture_normal::Shutdown();
    }

    Regtest_payment_code()
        : Regtest_fixture_normal(2)
        , api_server_1_(
              ot::Context().StartServer(OTTestEnvironment::Args(), 0, true))
        , alice_([&]() -> const ot::identity::Nym& {
            if (!alice_p_) {
                const auto reason =
                    client_1_.Factory().PasswordPrompt(__FUNCTION__);
                const auto& vector = vectors_3_.alice_;
                const auto seedID = [&] {
                    const auto words =
                        client_1_.Factory().SecretFromText(vector.words_);
                    const auto phrase = client_1_.Factory().Secret(0);

                    return client_1_.Seeds().ImportSeed(
                        words,
                        phrase,
                        ot::crypto::SeedStyle::BIP39,
                        ot::crypto::Language::en,
                        reason);
                }();

                alice_p_ = client_1_.Wallet().Nym(reason, "Alice", {seedID, 0});

                OT_ASSERT(alice_p_)
                OT_ASSERT(alice_p_->PaymentCode() == vector.payment_code_)

                client_1_.Blockchain().NewHDSubaccount(
                    alice_p_->ID(),
                    ot::BlockchainAccountType::BIP44,
                    test_chain_,
                    reason);
            }

            OT_ASSERT(alice_p_)

            return *alice_p_;
        }())
        , bob_([&]() -> const ot::identity::Nym& {
            if (!bob_p_) {
                const auto reason =
                    client_2_.Factory().PasswordPrompt(__FUNCTION__);
                const auto& vector = vectors_3_.bob_;
                const auto seedID = [&] {
                    const auto words =
                        client_2_.Factory().SecretFromText(vector.words_);
                    const auto phrase = client_2_.Factory().Secret(0);

                    return client_2_.Seeds().ImportSeed(
                        words,
                        phrase,
                        ot::crypto::SeedStyle::BIP39,
                        ot::crypto::Language::en,
                        reason);
                }();

                bob_p_ = client_2_.Wallet().Nym(reason, "Bob", {seedID, 0});

                OT_ASSERT(bob_p_)

                client_2_.Blockchain().NewHDSubaccount(
                    bob_p_->ID(),
                    ot::BlockchainAccountType::BIP44,
                    test_chain_,
                    reason);
            }

            OT_ASSERT(bob_p_)

            return *bob_p_;
        }())
        , expected_notary_(client_1_.UI().BlockchainNotaryID(test_chain_))
        , expected_unit_(client_1_.UI().BlockchainUnitID(test_chain_))
        , expected_display_unit_(u8"UNITTEST")
        , expected_account_name_(u8"This device")
        , expected_notary_name_(u8"Unit Test Simulation")
        , memo_outgoing_("memo for outgoing transaction")
        , expected_account_type_(ot::AccountType::Blockchain)
        , expected_unit_type_(ot::contact::ContactItemType::Regtest)
        , mine_to_alice_([&](Height height) -> Transaction {
            using OutputBuilder = ot::api::Factory::OutputBuilder;
            static constexpr auto baseAmmount =
                ot::blockchain::Amount{10000000000};
            auto meta = std::vector<OutpointMetadata>{};

            auto output = miner_.Factory().BitcoinGenerationTransaction(
                test_chain_,
                height,
                [&] {
                    auto output = std::vector<OutputBuilder>{};
                    const auto& account = SendHD();
                    const auto reason =
                        client_1_.Factory().PasswordPrompt(__FUNCTION__);
                    const auto keys = std::set<bca::Key>{};
                    const auto index =
                        account.Reserve(Subchain::External, reason);

                    EXPECT_TRUE(index.has_value());

                    const auto& element = account.BalanceElement(
                        Subchain::External, index.value_or(0));
                    const auto key = element.Key();

                    OT_ASSERT(key);

                    const auto& [bytes, value, pattern] = meta.emplace_back(
                        client_1_.Factory().Data(element.Key()->PublicKey()),
                        baseAmmount,
                        Pattern::PayToPubkey);
                    output.emplace_back(
                        value,
                        miner_.Factory().BitcoinScriptP2PK(test_chain_, *key),
                        keys);

                    return output;
                }(),
                coinbase_fun_);

            OT_ASSERT(output);

            const auto& txid = transactions_.emplace_back(output->ID()).get();
            auto& [bytes, amount, pattern] = meta.at(0);
            expected_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(txid.Bytes(), 0),
                std::forward_as_tuple(
                    std::move(bytes), std::move(amount), std::move(pattern)));

            return output;
        })
        , listener_alice_([&]() -> ScanListener& {
            if (!listener_alice_p_) {
                listener_alice_p_ = std::make_unique<ScanListener>(client_1_);
            }

            OT_ASSERT(listener_alice_p_);

            return *listener_alice_p_;
        }())
        , listener_bob_([&]() -> ScanListener& {
            if (!listener_bob_p_) {
                listener_bob_p_ = std::make_unique<ScanListener>(client_2_);
            }

            OT_ASSERT(listener_bob_p_);

            return *listener_bob_p_;
        }())
    {
        if (false == init_) {
            server_1_.init(api_server_1_);
            set_introduction_server(miner_, server_1_);
            set_introduction_server(client_1_, server_1_);
            set_introduction_server(client_2_, server_1_);
            init_ = true;
        }
    }
};

ot::Nym_p Regtest_payment_code::alice_p_{};
ot::Nym_p Regtest_payment_code::bob_p_{};
Regtest_payment_code::Transactions Regtest_payment_code::transactions_{};
std::unique_ptr<ScanListener> Regtest_payment_code::listener_alice_p_{};
std::unique_ptr<ScanListener> Regtest_payment_code::listener_bob_p_{};
Regtest_payment_code::Expected Regtest_payment_code::expected_{};

TEST_F(Regtest_payment_code, init_opentxs) {}

TEST_F(Regtest_payment_code, start_chains) { EXPECT_TRUE(Start()); }

TEST_F(Regtest_payment_code, connect_peers) { EXPECT_TRUE(Connect()); }

TEST_F(Regtest_payment_code, init_account_activity)
{
    account_activity_alice_.expected_ += 0;
    account_activity_bob_.expected_ += 0;
    client_1_.UI().AccountActivity(
        alice_.ID(),
        SendHD().Parent().AccountID(),
        make_cb(account_activity_alice_, u8"account_activity_alice"));
    client_2_.UI().AccountActivity(
        bob_.ID(),
        ReceiveHD().Parent().AccountID(),
        make_cb(account_activity_bob_, u8"account_activity_bob"));
    wait_for_counter(account_activity_alice_);
    wait_for_counter(account_activity_bob_);
}

TEST_F(Regtest_payment_code, mine)
{
    auto future = listener_alice_.get_future(SendHD(), Subchain::External, 1);
    constexpr auto count{1};
    account_activity_alice_.expected_ += (2 * count);

    EXPECT_TRUE(Mine(0, count, mine_to_alice_));
    EXPECT_TRUE(listener_alice_.wait(future));
}

TEST_F(Regtest_payment_code, first_block)
{
    const auto& blockchain =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    const auto blockHash = blockchain.HeaderOracle().BestHash(1);

    ASSERT_FALSE(blockHash->empty());

    const auto pBlock = blockchain.BlockOracle().LoadBitcoin(blockHash).get();

    ASSERT_TRUE(pBlock);

    const auto& block = *pBlock;

    ASSERT_EQ(block.size(), 1);

    const auto pTx = block.at(0);

    ASSERT_TRUE(pTx);

    const auto& tx = *pTx;

    EXPECT_EQ(tx.ID(), transactions_.at(0));
    EXPECT_EQ(tx.BlockPosition(), 0);
    ASSERT_EQ(tx.Outputs().size(), 1);
}

TEST_F(Regtest_payment_code, alice_after_receive_wallet)
{
    const auto& network =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();
    const auto& nym = alice_.ID();
    const auto& account = SendHD().ID();
    const auto blankNym = client_1_.Factory().NymID();
    const auto blankAccount = client_1_.Factory().Identifier();
    using Balance = ot::blockchain::Balance;
    const auto balance = Balance{10000000000, 10000000000};
    const auto noBalance = Balance{0, 0};

    EXPECT_EQ(wallet.GetBalance(), balance);
    EXPECT_EQ(network.GetBalance(), balance);
    EXPECT_EQ(wallet.GetBalance(nym), balance);
    EXPECT_EQ(network.GetBalance(nym), balance);
    EXPECT_EQ(wallet.GetBalance(nym, account), balance);
    EXPECT_EQ(wallet.GetBalance(blankNym), noBalance);
    EXPECT_EQ(network.GetBalance(blankNym), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(nym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, account), noBalance);

    using TxoState = ot::blockchain::node::Wallet::TxoState;
    auto type = TxoState::All;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, account, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, account, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, account, type)));

    type = TxoState::UnconfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, account, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, account, type).size(), 0u);

    type = TxoState::UnconfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, account, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, account, type).size(), 0u);

    type = TxoState::ConfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, account, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, account, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, account, type)));

    type = TxoState::ConfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, account, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, account, type).size(), 0u);

    type = TxoState::OrphanedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, account, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, account, type).size(), 0u);

    type = TxoState::OrphanedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, account, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, account, type).size(), 0u);
}

TEST_F(Regtest_payment_code, alice_after_receive_ui)
{
    wait_for_counter(account_activity_alice_);
    const auto& expectedAccount = SendHD().Parent().AccountID();
    const auto& widget =
        client_1_.UI().AccountActivity(alice_.ID(), expectedAccount);

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 10000000000);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"100 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{1, 1};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), 10000000000);
    EXPECT_EQ(row->Contacts().size(), 0);
    EXPECT_EQ(row->DisplayAmount(), u8"100 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(row->Text(), "Incoming Unit Test Simulation transaction");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(0)));
    EXPECT_TRUE(row->Last());
}

TEST_F(Regtest_payment_code, send_to_bob)
{
    account_activity_alice_.expected_ += 2;
    account_activity_bob_.expected_ += 2;
    const auto& network =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    auto future = network.SendToPaymentCode(
        alice_.ID(),
        client_1_.Factory().PaymentCode(vectors_3_.bob_.payment_code_),
        1000000000,
        memo_outgoing_);
    const auto& txid = transactions_.emplace_back(future.get().second);

    EXPECT_FALSE(txid->empty());

    {
        const auto& element = SendPC().BalanceElement(Subchain::Outgoing, 0);
        constexpr auto amount = ot::blockchain::Amount{1000000000};
        expected_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(txid->Bytes(), 0),
            std::forward_as_tuple(
                client_1_.Factory().Data(element.Key()->PublicKey()),
                amount,
                Pattern::PayToPubkey));
    }
    {
        const auto& element = SendHD().BalanceElement(Subchain::Internal, 0);
        constexpr auto amount = ot::blockchain::Amount{8999999684};
        expected_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(txid->Bytes(), 1),
            std::forward_as_tuple(
                client_1_.Factory().Data(element.Key()->PublicKey()),
                amount,
                Pattern::PayToMultisig));
    }
}

TEST_F(Regtest_payment_code, alice_after_unconfirmed_spend_wallet)
{
    const auto& network =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();
    const auto& nym = alice_.ID();
    const auto& accountHD = SendHD().ID();
    const auto& accountPC = SendPC().ID();
    const auto blankNym = client_1_.Factory().NymID();
    const auto blankAccount = client_1_.Factory().Identifier();
    using Balance = ot::blockchain::Balance;
    const auto balance = Balance{10000000000, 8999999684};
    const auto noBalance = Balance{0, 0};

    EXPECT_EQ(wallet.GetBalance(), balance);
    EXPECT_EQ(network.GetBalance(), balance);
    EXPECT_EQ(wallet.GetBalance(nym), balance);
    EXPECT_EQ(network.GetBalance(nym), balance);
    EXPECT_EQ(wallet.GetBalance(nym, accountHD), balance);
    EXPECT_EQ(wallet.GetBalance(nym, accountPC), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym), noBalance);
    EXPECT_EQ(network.GetBalance(blankNym), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(nym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountHD), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountPC), noBalance);

    using TxoState = ot::blockchain::node::Wallet::TxoState;
    auto type = TxoState::All;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 2u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 2u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 2u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::UnconfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::UnconfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::ConfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::ConfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::OrphanedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::OrphanedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);
}

TEST_F(Regtest_payment_code, alice_contact_consistency)
{
    const auto other = client_1_.Contacts().ContactID(bob_.ID());
    const auto expected = client_1_.Contacts().PaymentCodeToContact(
        client_1_.Factory().PaymentCode(vectors_3_.bob_.payment_code_),
        test_chain_);

    EXPECT_EQ(other, expected);
}

TEST_F(Regtest_payment_code, alice_after_unconfirmed_spend_ui)
{
    wait_for_counter(account_activity_alice_);
    const auto& expectedAccount = SendHD().Parent().AccountID();
    const auto& widget =
        client_1_.UI().AccountActivity(alice_.ID(), expectedAccount);
    const auto expectedContact = client_1_.Contacts().ContactID(bob_.ID());

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 8999999684);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"89.999 996 84 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{1, 1};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), -1000000316);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"-10.000 003 16 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(),
        "Outgoing Unit Test Simulation transaction to "
        "PD1jFsimY3DQUe7qGtx3z8BohTaT6r4kwJMCYXwp7uY8z6BSaFrpM");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(1)));
    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), 10000000000);
    EXPECT_EQ(row->Contacts().size(), 0);
    EXPECT_EQ(row->DisplayAmount(), u8"100 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(row->Text(), "Incoming Unit Test Simulation transaction");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(0)));
    EXPECT_TRUE(row->Last());
}

TEST_F(Regtest_payment_code, first_outgoing_transaction_alice)
{
    const auto& api = client_1_;
    const auto& blockchain = api.Blockchain();
    const auto& contact = api.Contacts();
    const auto& me = alice_.ID();
    const auto self = contact.ContactID(me);
    const auto other = contact.ContactID(bob_.ID());
    const auto& txid = transactions_.at(1).get();
    const auto& pTX = blockchain.LoadTransactionBitcoin(txid);

    ASSERT_TRUE(pTX);

    const auto& tx = *pTX;

    {
        const auto nyms = tx.AssociatedLocalNyms(blockchain);

        EXPECT_GT(nyms.size(), 0);

        if (0 < nyms.size()) {
            EXPECT_EQ(nyms.size(), 1);
            EXPECT_EQ(nyms.front(), me);
        }
    }
    {
        const auto contacts =
            tx.AssociatedRemoteContacts(blockchain, contact, me);

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);
            EXPECT_EQ(contacts.front(), other);
        }
    }
    {
        ASSERT_EQ(tx.Inputs().size(), 1u);

        const auto& script = tx.Inputs().at(0u).Script();

        ASSERT_EQ(script.size(), 1u);

        const auto& sig = script.at(0u);

        ASSERT_TRUE(sig.data_.has_value());
        EXPECT_GE(sig.data_.value().size(), 70u);
        EXPECT_LE(sig.data_.value().size(), 74u);
    }
    {
        ASSERT_EQ(tx.Outputs().size(), 2u);

        const auto& payment = tx.Outputs().at(0);
        const auto& change = tx.Outputs().at(1);

        EXPECT_EQ(payment.Payer(), self);
        EXPECT_EQ(payment.Payee(), other);
        EXPECT_EQ(change.Payer(), self);
        EXPECT_EQ(change.Payee(), self);
    }
}

TEST_F(Regtest_payment_code, first_unconfirmed_incoming_bob)
{
    wait_for_counter(account_activity_bob_);
    // TODO check wallet and ui state for bob
}

TEST_F(Regtest_payment_code, confirm_send)
{
    auto future1 = listener_alice_.get_future(SendHD(), Subchain::External, 2);
    auto future2 = listener_alice_.get_future(SendHD(), Subchain::Internal, 2);
    account_activity_alice_.expected_ += 3;
    account_activity_bob_.expected_ += 2;
    const auto& txid = transactions_.at(1).get();
    const auto extra = [&] {
        auto output = std::vector<Transaction>{};
        const auto& pTX = output.emplace_back(
            client_1_.Blockchain().LoadTransactionBitcoin(txid));

        OT_ASSERT(pTX);

        return output;
    }();

    EXPECT_TRUE(Mine(1, 1, default_, extra));
    EXPECT_TRUE(listener_alice_.wait(future1));
    EXPECT_TRUE(listener_alice_.wait(future2));
}

TEST_F(Regtest_payment_code, second_block)
{
    const auto& blockchain =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    const auto blockHash = blockchain.HeaderOracle().BestHash(2);
    auto expected = std::vector<ot::Space>{};

    ASSERT_FALSE(blockHash->empty());

    const auto pBlock = blockchain.BlockOracle().LoadBitcoin(blockHash).get();

    ASSERT_TRUE(pBlock);

    const auto& block = *pBlock;

    ASSERT_EQ(block.size(), 2);

    {
        const auto pTx = block.at(0);

        ASSERT_TRUE(pTx);

        const auto& tx = *pTx;
        expected.emplace_back(ot::space(tx.ID().Bytes()));

        EXPECT_EQ(tx.BlockPosition(), 0);
        EXPECT_EQ(tx.Outputs().size(), 1);
    }

    {
        const auto pTx = block.at(1);

        ASSERT_TRUE(pTx);

        const auto& tx = *pTx;
        expected.emplace_back(ot::space(tx.ID().Bytes()));

        EXPECT_EQ(tx.ID(), transactions_.at(1));
        EXPECT_EQ(tx.BlockPosition(), 1);

        ASSERT_EQ(tx.Inputs().size(), 1);

        {
            const auto& input = tx.Inputs().at(0);
            expected.emplace_back(ot::space(input.PreviousOutput().Bytes()));
        }

        ASSERT_EQ(tx.Outputs().size(), 2);

        {
            const auto& output = tx.Outputs().at(0);
            const auto& script = output.Script();

            ASSERT_EQ(script.Type(), Pattern::PayToPubkey);

            const auto bytes = script.Pubkey();

            ASSERT_TRUE(bytes.has_value());

            expected.emplace_back(ot::space(bytes.value()));
        }
        {
            const auto& output = tx.Outputs().at(1);
            const auto& script = output.Script();

            ASSERT_EQ(script.Type(), Pattern::PayToMultisig);

            for (auto i{0u}; i < 3u; ++i) {
                const auto bytes = script.MultisigPubkey(i);

                ASSERT_TRUE(bytes.has_value());

                expected.emplace_back(ot::space(bytes.value()));
            }
        }
    }

    {
        auto elements = block.ExtractElements(FilterType::ES);
        std::sort(elements.begin(), elements.end());
        std::sort(expected.begin(), expected.end());

        EXPECT_EQ(expected.size(), 7);
        EXPECT_EQ(elements.size(), expected.size());
        EXPECT_EQ(elements, expected);
    }

    const auto pFilter =
        blockchain.FilterOracle().LoadFilter(FilterType::ES, blockHash);

    ASSERT_TRUE(pFilter);

    const auto& filter = *pFilter;

    for (const auto& element : expected) {
        EXPECT_TRUE(filter.Test(ot::reader(element)));
    }
}

TEST_F(Regtest_payment_code, alice_after_confirmed_spend_wallet)
{
    const auto& network =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();
    const auto& nym = alice_.ID();
    const auto& accountHD = SendHD().ID();
    const auto& accountPC = SendPC().ID();
    const auto blankNym = client_1_.Factory().NymID();
    const auto blankAccount = client_1_.Factory().Identifier();
    using Balance = ot::blockchain::Balance;
    const auto balance = Balance{8999999684, 8999999684};
    const auto noBalance = Balance{0, 0};

    EXPECT_EQ(wallet.GetBalance(), balance);
    EXPECT_EQ(network.GetBalance(), balance);
    EXPECT_EQ(wallet.GetBalance(nym), balance);
    EXPECT_EQ(network.GetBalance(nym), balance);
    EXPECT_EQ(wallet.GetBalance(nym, accountHD), balance);
    EXPECT_EQ(wallet.GetBalance(nym, accountPC), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym), noBalance);
    EXPECT_EQ(network.GetBalance(blankNym), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(nym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountHD), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountPC), noBalance);

    using TxoState = ot::blockchain::node::Wallet::TxoState;
    auto type = TxoState::All;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 2u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 2u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 2u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::UnconfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::UnconfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::ConfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::ConfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::OrphanedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::OrphanedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);
}

TEST_F(Regtest_payment_code, alice_after_confirmed_spend_ui)
{
    wait_for_counter(account_activity_alice_, false);
    const auto& expectedAccount = SendHD().Parent().AccountID();
    const auto& widget =
        client_1_.UI().AccountActivity(alice_.ID(), expectedAccount);
    const auto expectedContact = client_1_.Contacts().ContactID(bob_.ID());

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 8999999684);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"89.999 996 84 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{2, 2};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), -1000000316);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"-10.000 003 16 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(),
        "Outgoing Unit Test Simulation transaction to "
        "PD1jFsimY3DQUe7qGtx3z8BohTaT6r4kwJMCYXwp7uY8z6BSaFrpM");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(1)));
    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), 10000000000);
    EXPECT_EQ(row->Contacts().size(), 0);
    EXPECT_EQ(row->DisplayAmount(), u8"100 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(row->Text(), "Incoming Unit Test Simulation transaction");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(0)));
    EXPECT_TRUE(row->Last());

    const auto& tree = client_1_.Blockchain().Account(alice_.ID(), test_chain_);
    const auto& pc = tree.GetPaymentCode();

    ASSERT_EQ(pc.size(), 1);

    const auto& account = pc.at(0);
    const auto lookahead = account.Lookahead() - 1;

    EXPECT_EQ(account.Type(), bca::SubaccountType::PaymentCode);
    EXPECT_TRUE(account.IsNotified());

    {
        constexpr auto subchain{Subchain::Incoming};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::Outgoing};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::External};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
    {
        constexpr auto subchain{Subchain::Internal};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
}

TEST_F(Regtest_payment_code, bob_contact_consistency)
{
    const auto other = client_2_.Contacts().ContactID(alice_.ID());
    const auto expected = client_2_.Contacts().PaymentCodeToContact(
        client_2_.Factory().PaymentCode(vectors_3_.alice_.payment_code_),
        test_chain_);

    EXPECT_EQ(other, expected);
}

TEST_F(Regtest_payment_code, bob_after_receive_ui)
{
    wait_for_counter(account_activity_bob_);
    const auto& expectedAccount = ReceiveHD().Parent().AccountID();
    const auto& widget =
        client_2_.UI().AccountActivity(bob_.ID(), expectedAccount);
    const auto expectedContact = client_2_.Contacts().ContactID(alice_.ID());

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 1000000000);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"10 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{2, 2};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), 1000000000);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"10 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(),
        "Incoming Unit Test Simulation transaction from "
        "PD1jTsa1rjnbMMLVbj5cg2c8KkFY32KWtPRqVVpSBkv1jf8zjHJVu");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(1)));
    EXPECT_TRUE(row->Last());

    const auto& tree = client_2_.Blockchain().Account(bob_.ID(), test_chain_);
    const auto& pc = tree.GetPaymentCode();

    ASSERT_EQ(pc.size(), 1);

    const auto& account = pc.at(0);
    const auto lookahead = account.Lookahead() - 1u;

    EXPECT_EQ(account.Type(), bca::SubaccountType::PaymentCode);
    EXPECT_FALSE(account.IsNotified());

    {
        constexpr auto subchain{Subchain::Incoming};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::Outgoing};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::External};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
    {
        constexpr auto subchain{Subchain::Internal};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
}

TEST_F(Regtest_payment_code, first_incoming_transaction_bob)
{
    const auto& api = client_2_;
    const auto& blockchain = api.Blockchain();
    const auto& contact = api.Contacts();
    const auto& me = bob_.ID();
    const auto self = contact.ContactID(me);
    const auto other = contact.ContactID(alice_.ID());
    const auto& txid = transactions_.at(1).get();
    const auto& pTX = blockchain.LoadTransactionBitcoin(txid);

    ASSERT_TRUE(pTX);

    const auto& tx = *pTX;

    {
        const auto nyms = tx.AssociatedLocalNyms(blockchain);

        EXPECT_GT(nyms.size(), 0);

        if (0 < nyms.size()) {
            EXPECT_EQ(nyms.size(), 1);
            EXPECT_EQ(nyms.front(), me);
        }
    }
    {
        const auto contacts =
            tx.AssociatedRemoteContacts(blockchain, contact, me);

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);
            EXPECT_EQ(contacts.front(), other);
        }
    }
    {
        ASSERT_EQ(tx.Inputs().size(), 1u);

        const auto& script = tx.Inputs().at(0u).Script();

        ASSERT_EQ(script.size(), 1u);

        const auto& sig = script.at(0u);

        ASSERT_TRUE(sig.data_.has_value());
        EXPECT_GE(sig.data_.value().size(), 70u);
        EXPECT_LE(sig.data_.value().size(), 74u);
    }
    {
        ASSERT_EQ(tx.Outputs().size(), 2u);

        const auto& payment = tx.Outputs().at(0);
        const auto& change = tx.Outputs().at(1);

        EXPECT_EQ(payment.Payer(), other);
        EXPECT_EQ(payment.Payee(), self);
        EXPECT_TRUE(change.Payer()->empty());
        EXPECT_TRUE(change.Payee()->empty());
    }
}

TEST_F(Regtest_payment_code, bob_after_receive_wallet)
{
    const auto& network =
        client_2_.Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();
    const auto& nym = bob_.ID();
    const auto& accountHD = ReceiveHD().ID();
    const auto& accountPC = ReceivePC().ID();
    const auto blankNym = client_2_.Factory().NymID();
    const auto blankAccount = client_2_.Factory().Identifier();
    using Balance = ot::blockchain::Balance;
    const auto balance = Balance{1000000000, 1000000000};
    const auto noBalance = Balance{0, 0};

    EXPECT_EQ(wallet.GetBalance(), balance);
    EXPECT_EQ(network.GetBalance(), balance);
    EXPECT_EQ(wallet.GetBalance(nym), balance);
    EXPECT_EQ(network.GetBalance(nym), balance);
    EXPECT_EQ(wallet.GetBalance(nym, accountHD), noBalance);
    EXPECT_EQ(wallet.GetBalance(nym, accountPC), balance);
    EXPECT_EQ(wallet.GetBalance(blankNym), noBalance);
    EXPECT_EQ(network.GetBalance(blankNym), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(nym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountHD), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountPC), noBalance);

    using TxoState = ot::blockchain::node::Wallet::TxoState;
    auto type = TxoState::All;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountPC, type)));

    type = TxoState::UnconfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::ConfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountPC, type)));

    type = TxoState::ConfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::OrphanedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::OrphanedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);
}

#if OT_WITH_RPC
TEST_F(Regtest_payment_code, rpc_account_list_alice)
{
    const auto index{client_1_.Instance()};
    const auto command = ot::rpc::request::ListAccounts{index};
    const auto base = ot_.RPC(command);
    const auto& response = base->asListAccounts();
    const auto& codes = response.ResponseCodes();
    const auto expected = [&] {
        auto out = std::set<std::string>{};
        const auto& id = client_1_.Blockchain()
                             .Account(alice_.ID(), test_chain_)
                             .AccountID();
        out.emplace(id.str());

        return out;
    }();
    const auto& ids = response.AccountIDs();

    ASSERT_EQ(codes.size(), 1);
    EXPECT_EQ(codes.at(0).first, 0);
    EXPECT_EQ(codes.at(0).second, ot::rpc::ResponseCode::success);
    EXPECT_EQ(ids.size(), 1);

    for (const auto& id : ids) { EXPECT_EQ(expected.count(id), 1); }
}

TEST_F(Regtest_payment_code, rpc_account_list_bob)
{
    const auto index{client_2_.Instance()};
    const auto command = ot::rpc::request::ListAccounts{index};
    const auto base = ot_.RPC(command);
    const auto& response = base->asListAccounts();
    const auto& codes = response.ResponseCodes();
    const auto expected = [&] {
        auto out = std::set<std::string>{};
        const auto& id =
            client_2_.Blockchain().Account(bob_.ID(), test_chain_).AccountID();
        out.emplace(id.str());

        return out;
    }();
    const auto& ids = response.AccountIDs();

    ASSERT_EQ(codes.size(), 1);
    EXPECT_EQ(codes.at(0).first, 0);
    EXPECT_EQ(codes.at(0).second, ot::rpc::ResponseCode::success);
    EXPECT_EQ(ids.size(), 1);

    for (const auto& id : ids) { EXPECT_EQ(expected.count(id), 1); }
}

// TODO more rpc tests go here
#endif  // OT_WITH_RPC

TEST_F(Regtest_payment_code, second_send_to_bob)
{
    account_activity_alice_.expected_ += 2;
    account_activity_bob_.expected_ += 2;
    const auto& network =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    auto future = network.SendToPaymentCode(
        alice_.ID(),
        client_1_.Factory().PaymentCode(vectors_3_.bob_.payment_code_),
        1500000000,
        memo_outgoing_);
    const auto& txid = transactions_.emplace_back(future.get().second);

    EXPECT_FALSE(txid->empty());

    {
        const auto& element = SendPC().BalanceElement(Subchain::Outgoing, 1);
        constexpr auto amount = ot::blockchain::Amount{1500000000};
        expected_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(txid->Bytes(), 0),
            std::forward_as_tuple(
                client_1_.Factory().Data(element.Key()->PublicKey()),
                amount,
                Pattern::PayToPubkey));
    }
    {
        const auto& element = SendHD().BalanceElement(Subchain::Internal, 1);
        constexpr auto amount = ot::blockchain::Amount{7499999448};
        expected_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(txid->Bytes(), 1),
            std::forward_as_tuple(
                element.PubkeyHash(), amount, Pattern::PayToPubkeyHash));
    }
}

TEST_F(Regtest_payment_code, alice_after_unconfirmed_second_spend_wallet)
{
    const auto& network =
        client_1_.Network().Blockchain().GetChain(test_chain_);
    const auto& wallet = network.Wallet();
    const auto& nym = alice_.ID();
    const auto& accountHD = SendHD().ID();
    const auto& accountPC = SendPC().ID();
    const auto blankNym = client_1_.Factory().NymID();
    const auto blankAccount = client_1_.Factory().Identifier();
    using Balance = ot::blockchain::Balance;
    const auto balance = Balance{8999999684, 7499999448};
    const auto noBalance = Balance{0, 0};

    EXPECT_EQ(wallet.GetBalance(), balance);
    EXPECT_EQ(network.GetBalance(), balance);
    EXPECT_EQ(wallet.GetBalance(nym), balance);
    EXPECT_EQ(network.GetBalance(nym), balance);
    EXPECT_EQ(wallet.GetBalance(nym, accountHD), balance);
    EXPECT_EQ(wallet.GetBalance(nym, accountPC), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym), noBalance);
    EXPECT_EQ(network.GetBalance(blankNym), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(nym, blankAccount), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountHD), noBalance);
    EXPECT_EQ(wallet.GetBalance(blankNym, accountPC), noBalance);

    using TxoState = ot::blockchain::node::Wallet::TxoState;
    auto type = TxoState::All;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 3u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 3u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 3u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::UnconfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::UnconfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::ConfirmedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::ConfirmedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 1u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, type)));
    EXPECT_TRUE(TestUTXOs(expected_, wallet.GetOutputs(nym, accountHD, type)));

    type = TxoState::OrphanedNew;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);

    type = TxoState::OrphanedSpend;

    EXPECT_EQ(wallet.GetOutputs(type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, accountPC, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(nym, blankAccount, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountHD, type).size(), 0u);
    EXPECT_EQ(wallet.GetOutputs(blankNym, accountPC, type).size(), 0u);
}

TEST_F(Regtest_payment_code, alice_after_unconfirmed_second_spend_ui)
{
    wait_for_counter(account_activity_alice_);
    const auto& expectedAccount = SendHD().Parent().AccountID();
    const auto& widget =
        client_1_.UI().AccountActivity(alice_.ID(), expectedAccount);
    const auto expectedContact = client_1_.Contacts().ContactID(bob_.ID());

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 7499999448);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"74.999 994 48 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{2, 2};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), -1500000236);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"-15.000 002 36 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(),
        "Outgoing Unit Test Simulation transaction to "
        "PD1jFsimY3DQUe7qGtx3z8BohTaT6r4kwJMCYXwp7uY8z6BSaFrpM");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);

    transactions_.emplace_back(
        ot::blockchain::NumberToHash(client_1_, row->UUID()));

    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), -1000000316);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"-10.000 003 16 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(),
        "Outgoing Unit Test Simulation transaction to "
        "PD1jFsimY3DQUe7qGtx3z8BohTaT6r4kwJMCYXwp7uY8z6BSaFrpM");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(1)));

    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), 10000000000);
    EXPECT_EQ(row->Contacts().size(), 0);
    EXPECT_EQ(row->DisplayAmount(), u8"100 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(row->Text(), "Incoming Unit Test Simulation transaction");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(0)));
    EXPECT_TRUE(row->Last());

    const auto& tree = client_1_.Blockchain().Account(alice_.ID(), test_chain_);
    const auto& pc = tree.GetPaymentCode();

    ASSERT_EQ(pc.size(), 1);

    const auto& account = pc.at(0);
    const auto lookahead = account.Lookahead() - 1u;

    EXPECT_EQ(account.Type(), bca::SubaccountType::PaymentCode);
    EXPECT_TRUE(account.IsNotified());

    {
        constexpr auto subchain{Subchain::Incoming};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::Outgoing};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::External};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
    {
        constexpr auto subchain{Subchain::Internal};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
}

TEST_F(Regtest_payment_code, second_outgoing_transaction_alice)
{
    const auto& api = client_1_;
    const auto& blockchain = api.Blockchain();
    const auto& contact = api.Contacts();
    const auto& me = alice_.ID();
    const auto self = contact.ContactID(me);
    const auto other = contact.ContactID(bob_.ID());
    const auto& txid = transactions_.at(2).get();
    const auto& pTX = blockchain.LoadTransactionBitcoin(txid);

    ASSERT_TRUE(pTX);

    const auto& tx = *pTX;

    {
        const auto nyms = tx.AssociatedLocalNyms(blockchain);

        EXPECT_GT(nyms.size(), 0);

        if (0 < nyms.size()) {
            EXPECT_EQ(nyms.size(), 1);
            EXPECT_EQ(nyms.front(), me);
        }
    }
    {
        const auto contacts =
            tx.AssociatedRemoteContacts(blockchain, contact, me);

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);
            EXPECT_EQ(contacts.front(), other);
        }
    }
    {
        ASSERT_EQ(tx.Inputs().size(), 1u);

        const auto& script = tx.Inputs().at(0u).Script();

        ASSERT_EQ(script.size(), 2u);

        const auto& placeholder = script.at(0u);
        using OP = ot::blockchain::block::bitcoin::OP;

        EXPECT_EQ(placeholder.opcode_, OP::ZERO);

        const auto& sig = script.at(1u);

        ASSERT_TRUE(sig.data_.has_value());
        EXPECT_GE(sig.data_.value().size(), 70u);
        EXPECT_LE(sig.data_.value().size(), 74u);
    }
    {
        ASSERT_EQ(tx.Outputs().size(), 2u);

        const auto& payment = tx.Outputs().at(0);
        const auto& change = tx.Outputs().at(1);

        EXPECT_EQ(payment.Payer(), self);
        EXPECT_EQ(payment.Payee(), other);
        EXPECT_EQ(change.Payer(), self);
        EXPECT_EQ(change.Payee(), self);
    }
}

TEST_F(Regtest_payment_code, bob_after_second_receive_ui)
{
    wait_for_counter(account_activity_bob_);
    const auto& expectedAccount = ReceiveHD().Parent().AccountID();
    const auto& widget =
        client_2_.UI().AccountActivity(bob_.ID(), expectedAccount);
    const auto expectedContact = client_2_.Contacts().ContactID(alice_.ID());

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 2500000000);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"25 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{2, 2};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), 1500000000);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"15 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(),
        "Incoming Unit Test Simulation transaction from "
        "PD1jTsa1rjnbMMLVbj5cg2c8KkFY32KWtPRqVVpSBkv1jf8zjHJVu");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(2)));
    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), 1000000000);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"10 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(),
        "Incoming Unit Test Simulation transaction from "
        "PD1jTsa1rjnbMMLVbj5cg2c8KkFY32KWtPRqVVpSBkv1jf8zjHJVu");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(1)));
    EXPECT_TRUE(row->Last());

    const auto& tree = client_2_.Blockchain().Account(bob_.ID(), test_chain_);
    const auto& pc = tree.GetPaymentCode();

    ASSERT_EQ(pc.size(), 1);

    const auto& account = pc.at(0);
    const auto lookahead = account.Lookahead() - 1u;

    EXPECT_EQ(account.Type(), bca::SubaccountType::PaymentCode);
    EXPECT_FALSE(account.IsNotified());

    {
        constexpr auto subchain{Subchain::Incoming};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::Outgoing};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::External};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
    {
        constexpr auto subchain{Subchain::Internal};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
}

TEST_F(Regtest_payment_code, update_contacts)
{
    account_activity_alice_.expected_ += 2;
    account_activity_bob_.expected_ += 2;
    client_1_.OTX().StartIntroductionServer(alice_.ID());
    client_2_.OTX().StartIntroductionServer(bob_.ID());
    auto task1 =
        client_1_.OTX().RegisterNymPublic(alice_.ID(), server_1_.id_, true);
    auto task2 =
        client_2_.OTX().RegisterNymPublic(bob_.ID(), server_1_.id_, true);

    ASSERT_NE(0, task1.first);
    ASSERT_NE(0, task2.first);
    EXPECT_EQ(
        ot::otx::LastReplyStatus::MessageSuccess, task1.second.get().first);
    EXPECT_EQ(
        ot::otx::LastReplyStatus::MessageSuccess, task2.second.get().first);

    client_1_.OTX().ContextIdle(alice_.ID(), server_1_.id_).get();
    client_2_.OTX().ContextIdle(bob_.ID(), server_1_.id_).get();
}

TEST_F(Regtest_payment_code, alice_account_activity_after_otx)
{
    wait_for_counter(account_activity_alice_);
    const auto& expectedAccount = SendHD().Parent().AccountID();
    const auto& widget =
        client_1_.UI().AccountActivity(alice_.ID(), expectedAccount);
    const auto expectedContact = client_1_.Contacts().ContactID(bob_.ID());

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 7499999448);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"74.999 994 48 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{2, 2};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), -1500000236);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"-15.000 002 36 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(row->Text(), "Outgoing Unit Test Simulation transaction to Bob");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);

    transactions_.emplace_back(
        ot::blockchain::NumberToHash(client_1_, row->UUID()));

    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), -1000000316);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"-10.000 003 16 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(row->Text(), "Outgoing Unit Test Simulation transaction to Bob");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(1)));

    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), 10000000000);
    EXPECT_EQ(row->Contacts().size(), 0);
    EXPECT_EQ(row->DisplayAmount(), u8"100 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(row->Text(), "Incoming Unit Test Simulation transaction");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(0)));
    EXPECT_TRUE(row->Last());
}

TEST_F(Regtest_payment_code, bob_account_activity_after_otx)
{
    wait_for_counter(account_activity_bob_);
    const auto& expectedAccount = ReceiveHD().Parent().AccountID();
    const auto& widget =
        client_2_.UI().AccountActivity(bob_.ID(), expectedAccount);
    const auto expectedContact = client_2_.Contacts().ContactID(alice_.ID());

    EXPECT_EQ(widget.AccountID(), expectedAccount.str());
    EXPECT_EQ(widget.Balance(), 2500000000);
    EXPECT_EQ(widget.BalancePolarity(), 1);
    EXPECT_EQ(widget.ContractID(), expected_unit_.str());
    EXPECT_FALSE(widget.DepositAddress().empty());
    EXPECT_FALSE(widget.DepositAddress(test_chain_).empty());
    EXPECT_TRUE(widget.DepositAddress(ot::blockchain::Type::Bitcoin).empty());

    const auto deposit = widget.DepositChains();

    ASSERT_EQ(deposit.size(), 1);
    EXPECT_EQ(deposit.at(0), test_chain_);
    EXPECT_EQ(widget.DisplayBalance(), u8"25 units");
    EXPECT_EQ(widget.DisplayUnit(), expected_display_unit_);
    EXPECT_EQ(widget.Name(), expected_account_name_);
    EXPECT_EQ(widget.NotaryID(), expected_notary_.str());
    EXPECT_EQ(widget.NotaryName(), expected_notary_name_);
    EXPECT_EQ(widget.SyncPercentage(), 100);

    constexpr auto progress = std::pair<int, int>{2, 2};

    EXPECT_EQ(widget.SyncProgress(), progress);
    EXPECT_EQ(widget.Type(), expected_account_type_);
    EXPECT_EQ(widget.Unit(), expected_unit_type_);

    auto row = widget.First();

    ASSERT_TRUE(row->Valid());
    EXPECT_EQ(row->Amount(), 1500000000);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"15 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(), "Incoming Unit Test Simulation transaction from Alice");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(2)));
    ASSERT_FALSE(row->Last());

    row = widget.Next();

    EXPECT_EQ(row->Amount(), 1000000000);

    {
        const auto contacts = row->Contacts();

        EXPECT_GT(contacts.size(), 0);

        if (0 < contacts.size()) {
            EXPECT_EQ(contacts.size(), 1);

            const auto& contact = contacts.front();

            EXPECT_EQ(contact, expectedContact->str());
        }
    }

    EXPECT_EQ(row->DisplayAmount(), u8"10 units");
    EXPECT_EQ(row->Memo(), "");
    EXPECT_EQ(row->Workflow(), "");
    EXPECT_EQ(
        row->Text(), "Incoming Unit Test Simulation transaction from Alice");
    EXPECT_EQ(row->Type(), ot::StorageBox::BLOCKCHAIN);
    EXPECT_EQ(row->UUID(), ot::blockchain::HashToNumber(transactions_.at(1)));
    EXPECT_TRUE(row->Last());

    const auto& tree = client_2_.Blockchain().Account(bob_.ID(), test_chain_);
    const auto& pc = tree.GetPaymentCode();

    ASSERT_EQ(pc.size(), 1);

    const auto& account = pc.at(0);
    const auto lookahead = account.Lookahead() - 1u;

    EXPECT_EQ(account.Type(), bca::SubaccountType::PaymentCode);
    EXPECT_FALSE(account.IsNotified());

    {
        constexpr auto subchain{Subchain::Incoming};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::Outgoing};

        const auto gen = account.LastGenerated(subchain);

        ASSERT_TRUE(gen.has_value());
        EXPECT_EQ(gen.value(), lookahead);
    }
    {
        constexpr auto subchain{Subchain::External};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
    {
        constexpr auto subchain{Subchain::Internal};

        const auto gen = account.LastGenerated(subchain);

        EXPECT_FALSE(gen.has_value());
    }
}

TEST_F(Regtest_payment_code, shutdown) { Shutdown(); }
}  // namespace ottest
