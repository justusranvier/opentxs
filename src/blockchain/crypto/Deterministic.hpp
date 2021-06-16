// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/container/flat_map.hpp>
#include <atomic>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Proto.hpp"
#include "blockchain/crypto/Subaccount.hpp"
#include "internal/blockchain/crypto/Crypto.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/Version.hpp"
#include "opentxs/blockchain/crypto/Deterministic.hpp"
#include "opentxs/blockchain/crypto/Subchain.hpp"
#include "opentxs/blockchain/crypto/Types.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/crypto/Types.hpp"
#include "opentxs/crypto/key/HD.hpp"
#include "opentxs/protobuf/BlockchainDeterministicAccountData.pb.h"
#include "opentxs/protobuf/HDPath.pb.h"

namespace opentxs
{
namespace api
{
namespace internal
{
struct Core;
}  // namespace internal
}  // namespace api

namespace proto
{
class BlockchainDeterministicAccountData;
}  // namespace proto

class PasswordPrompt;
}  // namespace opentxs

namespace opentxs::blockchain::crypto::implementation
{
class Deterministic : virtual public internal::Deterministic, public Subaccount
{
public:
    auto Floor(const Subchain type) const noexcept
        -> std::optional<Bip32Index> final;
    auto BalanceElement(const Subchain type, const Bip32Index index) const
        noexcept(false) -> const crypto::Element& final;
    auto GenerateNext(const Subchain type, const PasswordPrompt& reason)
        const noexcept -> std::optional<Bip32Index> final;
    auto Key(const Subchain type, const Bip32Index index) const noexcept
        -> ECKey final;
    auto LastGenerated(const Subchain type) const noexcept
        -> std::optional<Bip32Index> final;
    auto Lookahead() const noexcept -> std::size_t final { return window_; }
    auto Path() const noexcept -> proto::HDPath final { return path_; }
    auto PathRoot() const noexcept -> const std::string final
    {
        return path_.root();
    }
    auto Reserve(
        const Subchain type,
        const PasswordPrompt& reason,
        const Identifier& contact,
        const std::string& label,
        const Time time) const noexcept -> std::optional<Bip32Index> final;
    auto Reserve(
        const Subchain type,
        const std::size_t batch,
        const PasswordPrompt& reason,
        const Identifier& contact,
        const std::string& label,
        const Time time) const noexcept -> Batch override;
    auto RootNode(const PasswordPrompt& reason) const noexcept
        -> blockchain::crypto::HDKey override;

    ~Deterministic() override = default;

protected:
    using IndexMap = std::map<Subchain, Bip32Index>;
    using SerializedType = proto::BlockchainDeterministicAccountData;

    struct ChainData {
        AddressData internal_{};
        AddressData external_{};
    };

    static constexpr Bip32Index window_{20u};
    static constexpr Bip32Index max_allocation_{2000u};
    static constexpr Bip32Index max_index_{2147483648u};

    const proto::HDPath path_;
    mutable ChainData data_;
    mutable IndexMap generated_;
    mutable IndexMap used_;
    mutable boost::container::flat_map<Subchain, std::optional<Bip32Index>>
        last_allocation_;

    auto check_lookahead(
        const rLock& lock,
        const Subchain type,
        Batch& generated,
        const PasswordPrompt& reason) const noexcept(false) -> void;
    auto element(const rLock& lock, const Subchain type, const Bip32Index index)
        const noexcept(false) -> const internal::Element&
    {
        return const_cast<Deterministic*>(this)->element(lock, type, index);
    }
    auto is_generated(const rLock&, const Subchain type, Bip32Index index)
        const noexcept
    {
        return index < generated_.at(type);
    }
    auto need_lookahead(const rLock& lock, const Subchain type) const noexcept
        -> Bip32Index;
    auto serialize_deterministic(const rLock& lock, SerializedType& out)
        const noexcept -> void;
#if OT_CRYPTO_WITH_BIP32
    auto use_next(
        const rLock& lock,
        const Subchain type,
        const PasswordPrompt& reason,
        const Identifier& contact,
        const std::string& label,
        const Time time,
        Batch& generated) const noexcept -> std::optional<Bip32Index>;
#endif  // OT_CRYPTO_WITH_BIP32

    auto element(
        const rLock& lock,
        const Subchain type,
        const Bip32Index index) noexcept(false) -> internal::Element&;
    using Subaccount::init;
    auto init(const PasswordPrompt& reason) noexcept(false) -> void;

    Deterministic(
        const api::internal::Core& api,
        const internal::Account& parent,
        const SubaccountType type,
        OTIdentifier&& id,
        const proto::HDPath path,
        ChainData&& data,
        Identifier& out) noexcept;
    Deterministic(
        const api::internal::Core& api,
        const internal::Account& parent,
        const SubaccountType type,
        const SerializedType& serialized,
        const Bip32Index generated,
        const Bip32Index used,
        ChainData&& data,
        Identifier& out) noexcept(false);

private:
    using Status = internal::Element::Availability;
    using Fallback = std::map<Status, std::set<Bip32Index>>;
    using CachedKey = std::pair<std::mutex, blockchain::crypto::HDKey>;

    static constexpr auto BlockchainDeterministicAccountDataVersion =
        VersionNumber{1};

    mutable CachedKey cached_key_;

    static auto extract_contacts(
        const Bip32Index index,
        const AddressMap& map,
        std::set<OTIdentifier>& contacts) noexcept -> void;

#if OT_CRYPTO_WITH_BIP32
    auto accept(
        const rLock& lock,
        const Subchain type,
        const Identifier& contact,
        const std::string& label,
        const Time time,
        const Bip32Index index,
        Batch& generated,
        const PasswordPrompt& reason) const noexcept
        -> std::optional<Bip32Index>;
    auto check(
        const rLock& lock,
        const Subchain type,
        const Identifier& contact,
        const std::string& label,
        const Time time,
        const Bip32Index index,
        const PasswordPrompt& reason,
        Fallback& fallback,
        std::size_t& gap,
        Batch& generated) const noexcept(false) -> std::optional<Bip32Index>;
#endif  // OT_CRYPTO_WITH_BIP32
    auto check_activity(
        const rLock& lock,
        const std::vector<Activity>& unspent,
        std::set<OTIdentifier>& contacts,
        const PasswordPrompt& reason) const noexcept -> bool final;
    auto check_lookahead(
        const rLock& lock,
        Batch& generated,
        const PasswordPrompt& reason) const noexcept(false) -> void;
    auto confirm(
        const rLock& lock,
        const Subchain type,
        const Bip32Index index) noexcept -> void final;
    [[nodiscard]] auto finish_allocation(const rLock& lock, Batch& generated)
        const noexcept -> bool;
    [[nodiscard]] auto generate(
        const rLock& lock,
        const Subchain type,
        const Bip32Index index,
        const PasswordPrompt& reason) const noexcept(false) -> Bip32Index;
    [[nodiscard]] auto generate_next(
        const rLock& lock,
        const Subchain type,
        const PasswordPrompt& reason) const noexcept(false) -> Bip32Index;
    auto mutable_element(
        const rLock& lock,
        const Subchain type,
        const Bip32Index index) noexcept(false) -> internal::Element& final;
    virtual auto set_deterministic_contact(internal::Element&) const noexcept
        -> void
    {
    }
    virtual auto set_deterministic_contact(
        std::set<OTIdentifier>&) const noexcept -> void
    {
    }
    auto set_metadata(
        const rLock& lock,
        const Subchain subchain,
        const Bip32Index index,
        const Identifier& contact,
        const std::string& label) const noexcept -> void;
    auto unconfirm(
        const rLock& lock,
        const Subchain type,
        const Bip32Index index) noexcept -> void final;

    Deterministic() = delete;
    Deterministic(const Deterministic&) = delete;
    Deterministic(Deterministic&&) = delete;
    auto operator=(const Deterministic&) -> Deterministic& = delete;
    auto operator=(Deterministic&&) -> Deterministic& = delete;
};
}  // namespace opentxs::blockchain::crypto::implementation
