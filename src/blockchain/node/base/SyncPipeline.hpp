// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <string>

namespace opentxs
{
namespace api
{
namespace internal
{
struct Core;
}  // namespace internal

namespace network
{
namespace internal
{
struct Blockchain;
}  // namespace internal
}  // namespace network
}  // namespace api

namespace blockchain
{
namespace node
{
namespace internal
{
struct Manager;
struct FilterOracle;
struct HeaderOracle;
}  // namespace internal
}  // namespace node
}  // namespace blockchain

namespace network
{
namespace zeromq
{
class Message;
}  // namespace zeromq
}  // namespace network
}  // namespace opentxs

namespace opentxs::blockchain::node::base
{
class SyncPipeline
{
public:
    static auto ProcessThreadPool(const network::zeromq::Message& task) noexcept
        -> void;

    auto Init() const noexcept -> void;

    SyncPipeline(
        const api::internal::Core& api,
        const node::internal::Manager& parent,
        node::internal::HeaderOracle& header,
        const node::internal::FilterOracle& filter,
        const std::string& endpoint) noexcept;

    ~SyncPipeline();

private:
    struct Imp;

    std::unique_ptr<Imp> imp_;

    SyncPipeline() = delete;
    SyncPipeline(const SyncPipeline&) = delete;
    SyncPipeline(SyncPipeline&&) = delete;
    auto operator=(const SyncPipeline&) -> SyncPipeline& = delete;
    auto operator=(SyncPipeline&&) -> SyncPipeline& = delete;
};
}  // namespace opentxs::blockchain::node::base
