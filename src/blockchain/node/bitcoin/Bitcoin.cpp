// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                         // IWYU pragma: associated
#include "1_Internal.hpp"                       // IWYU pragma: associated
#include "blockchain/node/bitcoin/Bitcoin.hpp"  // IWYU pragma: associated

#include "Proto.tpp"
#include "internal/blockchain/block/Block.hpp"
#include "internal/blockchain/block/bitcoin/Bitcoin.hpp"
#include "internal/blockchain/node/Factory.hpp"
#include "internal/blockchain/node/Node.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/protobuf/BlockchainBlockHeader.pb.h"  // IWYU pragma: keep

// #define OT_METHOD "opentxs::blockchain::node::base::Network::"

namespace opentxs::factory
{
auto BlockchainNetworkBitcoin(
    const api::internal::Core& api,
    const api::client::internal::Blockchain& crypto,
    const api::network::internal::Blockchain& network,
    const blockchain::Type type,
    const blockchain::node::internal::Config& config,
    const std::string& seednode,
    const std::string& syncEndpoint) noexcept
    -> std::unique_ptr<blockchain::node::internal::Manager>
{
    using ReturnType = blockchain::node::base::Bitcoin;

    return std::make_unique<ReturnType>(
        api, crypto, network, type, config, seednode, syncEndpoint);
}

auto BlockchainNetworkBitcoin(
    const api::Core& api,
    const api::client::internal::Blockchain& crypto,
    const blockchain::Type type,
    const blockchain::node::internal::Config& config,
    const std::string& seednode,
    const std::string& syncEndpoint) noexcept
    -> std::unique_ptr<blockchain::node::internal::Manager>
{
    return BlockchainNetworkBitcoin(
        dynamic_cast<const api::internal::Core&>(api),
        crypto,
        api.Network().Blockchain().Internal(),
        type,
        config,
        seednode,
        syncEndpoint);
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::base
{
Bitcoin::Bitcoin(
    const api::internal::Core& api,
    const api::client::internal::Blockchain& crypto,
    const api::network::internal::Blockchain& network,
    const Type type,
    const internal::Config& config,
    const std::string& seednode,
    const std::string& syncEndpoint)
    : ot_super(api, crypto, network, type, config, seednode, syncEndpoint)
{
    init();
}

auto Bitcoin::instantiate_header(const ReadView payload) const noexcept
    -> std::unique_ptr<block::Header>
{
    using Type = block::Header::SerializedType;

    return std::unique_ptr<block::Header>{
        factory::BitcoinBlockHeader(api_, proto::Factory<Type>(payload))};
}

Bitcoin::~Bitcoin() { Shutdown(); }
}  // namespace opentxs::blockchain::node::base
