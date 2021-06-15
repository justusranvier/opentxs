// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                              // IWYU pragma: associated
#include "1_Internal.hpp"                            // IWYU pragma: associated
#include "opentxs/network/blockchain/sync/Data.hpp"  // IWYU pragma: associated

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "Proto.tpp"
#include "network/blockchain/sync/Base.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/blockchain/sync/MessageType.hpp"
#include "opentxs/network/blockchain/sync/State.hpp"
#include "opentxs/protobuf/BlockchainP2PSync.pb.h"
#include "opentxs/protobuf/Check.hpp"
#include "opentxs/protobuf/verify/BlockchainP2PSync.hpp"

#define OT_METHOD "opentxs::network::blockchain::sync::Data::"

namespace opentxs::network::blockchain::sync
{
struct DataImp final : public Base::Imp {
    const Data& parent_;

    auto asData() const noexcept -> const Data& final { return parent_; }

    DataImp(
        const Data& parent,
        WorkType type,
        sync::State state,
        Data::SyncData blocks,
        ReadView cfheader) noexcept(false)
        : Imp(
              Imp::default_version_,
              translate(type),
              [&] {
                  auto out = std::vector<State>{};
                  out.emplace_back(std::move(state));

                  return out;
              }(),
              std::string{cfheader},
              std::move(blocks))
        , parent_(parent)
    {
        switch (type_) {
            case MessageType::sync_reply: {
            } break;
            case MessageType::new_block_header: {
                if (0 == blocks_.size()) {
                    throw std::runtime_error{"Missing blocks"};
                }
            } break;
            default: {

                throw std::runtime_error{"Incorrect type"};
            }
        }
    }

private:
    DataImp() noexcept;
    DataImp(const DataImp&) = delete;
    DataImp(DataImp&&) = delete;
    auto operator=(const DataImp&) -> DataImp& = delete;
    auto operator=(DataImp&&) -> DataImp& = delete;
};

Data::Data(
    WorkType type,
    sync::State state,
    SyncData blocks,
    ReadView cfheader) noexcept(false)
    : Base(std::make_unique<DataImp>(
          *this,
          type,
          std::move(state),
          std::move(blocks),
          cfheader))
{
}

Data::Data() noexcept
    : Base(std::make_unique<Imp>())
{
}

auto Data::Add(ReadView data) noexcept -> bool
{
    const auto proto = proto::Factory<proto::BlockchainP2PSync>(data);

    if (false == proto::Validate(proto, VERBOSE)) { return false; }

    auto& blocks = const_cast<std::vector<Block>&>(imp_->blocks_);

    if (0 < blocks.size()) {
        const auto expected = blocks.back().Height() + 1;
        const auto height =
            static_cast<opentxs::blockchain::block::Height>(proto.height());

        if (height != expected) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Non-contiguous sync data")
                .Flush();

            return false;
        }
    }

    try {
        blocks.emplace_back(proto);

        return true;
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return false;
    }
}

auto Data::Add(Data&& data) noexcept -> bool
{
    auto& blocks = const_cast<std::vector<Block>&>(imp_->blocks_);
    auto& rhs = const_cast<std::vector<Block>&>(data.imp_->blocks_);

    if (0 == rhs.size()) { return true; }

    if (0 == blocks.size()) {
        std::move(rhs.begin(), rhs.end(), std::back_inserter(blocks));

        return true;
    }

    try {
        auto begin = [&] {
            const auto back = blocks.back().Height();
            const auto target = back + 1;
            const auto first = rhs.front().Height();

            if (target == first) {
                // NOTE Most common case. Incoming data starts exactly one block
                // after existing data.

                return rhs.begin();
            }

            if (target < first) {
                // NOTE Incoming data starts more than one block after existing
                // data. It's impossible to merge these items.

                throw std::runtime_error{"Non-contiguous range"};
            }

            const auto front = blocks.front().Height();

            if (front <= first) {
                // NOTE Incoming data starts before existing data. This might
                // happen due to a reorg.
                blocks.clear();

                return rhs.begin();
            }

            // NOTE Incoming data starts in the middle of existing data.
            // Existing blocks are presumed to be invalid due to reorg and are
            // erased.
            const auto overlap = first - front;
            blocks.erase(std::next(blocks.begin(), overlap), blocks.end());

            return rhs.begin();
        }();
        std::move(begin, rhs.end(), std::back_inserter(blocks));

        return true;
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return false;
    }
}

auto Data::Blocks() const noexcept -> const SyncData& { return imp_->blocks_; }

auto Data::LastPosition(const api::Core& api) const noexcept -> Position
{
    static const auto blank = Position{-1, api.Factory().Data()};
#if OT_BLOCKCHAIN
    const auto& blocks = imp_->blocks_;

    if (0u == blocks.size()) { return blank; }

    const auto& last = blocks.back();
    const auto header = api.Factory().BlockHeader(last.Chain(), last.Header());

    if (!header) { return blank; }

    return {last.Height(), header->Hash()};
#else

    return blank;
#endif  // OT_BLOCKCHAIN
}

auto Data::PreviousCfheader() const noexcept -> ReadView
{
    return imp_->endpoint_;
}

auto Data::State() const noexcept -> const sync::State&
{
    OT_ASSERT(0 < imp_->state_.size());

    return imp_->state_.front();
}

Data::~Data() = default;
}  // namespace opentxs::network::blockchain::sync
