// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/database/common/BlockFilter.hpp"  // IWYU pragma: associated

#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "Proto.tpp"
#include "internal/api/Api.hpp"  // IWYU pragma: keep
#include "internal/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/protobuf/BlockchainFilterHeader.pb.h"
#include "opentxs/protobuf/GCS.pb.h"
#include "util/LMDB.hpp"

#define OT_METHOD "opentxs::blockchain::database::common::BlockFilter::"

namespace opentxs::blockchain::database::common
{
BlockFilter::BlockFilter(
    const api::Core& api,
    opentxs::storage::lmdb::LMDB& lmdb) noexcept(false)
    : api_(api)
    , lmdb_(lmdb)
{
}

auto BlockFilter::HaveFilter(const FilterType type, const ReadView blockHash)
    const noexcept -> bool
{
    try {
        return lmdb_.Exists(translate_filter(type), blockHash);
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return false;
    }
}

auto BlockFilter::HaveFilterHeader(
    const FilterType type,
    const ReadView blockHash) const noexcept -> bool
{
    try {
        return lmdb_.Exists(translate_header(type), blockHash);
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

        return false;
    }
}

auto BlockFilter::LoadFilter(const FilterType type, const ReadView blockHash)
    const noexcept -> std::unique_ptr<const opentxs::blockchain::node::GCS>
{
    auto output = std::unique_ptr<const opentxs::blockchain::node::GCS>{};
    auto cb = [this, &output](const auto in) {
        if ((nullptr == in.data()) || (0 == in.size())) { return; }

        output = factory::GCS(api_, proto::Factory<proto::GCS>(in));
    };

    try {
        lmdb_.Load(translate_filter(type), blockHash, cb);
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();
    }

    return output;
}

auto BlockFilter::LoadFilterHash(
    const FilterType type,
    const ReadView blockHash,
    const AllocateOutput filterHash) const noexcept -> bool
{
    auto output{false};
    auto cb = [&output, &filterHash](const auto in) {
        auto size = in.size();

        if ((nullptr == in.data()) || (0 == size)) { return; }

        auto proto =
            proto::Factory<proto::BlockchainFilterHeader>(in.data(), in.size());
        const auto& field = proto.hash();
        auto bytes = filterHash(field.size());

        if (bytes.valid(field.size())) {
            std::memcpy(bytes, field.data(), bytes);
            output = true;
        }
    };

    try {
        lmdb_.Load(translate_header(type), blockHash, cb);
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();
    }

    return output;
}

auto BlockFilter::LoadFilterHeader(
    const FilterType type,
    const ReadView blockHash,
    const AllocateOutput header) const noexcept -> bool
{
    auto output{false};
    auto cb = [&output, &header](const auto in) {
        auto size = in.size();

        if ((nullptr == in.data()) || (0 == size)) { return; }

        auto proto =
            proto::Factory<proto::BlockchainFilterHeader>(in.data(), in.size());
        const auto& field = proto.header();
        auto bytes = header(field.size());

        if (bytes.valid(field.size())) {
            std::memcpy(bytes, field.data(), bytes);
            output = true;
        }
    };

    try {
        lmdb_.Load(translate_header(type), blockHash, cb);
    } catch (const std::exception& e) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();
    }

    return output;
}

auto BlockFilter::StoreFilterHeaders(
    const FilterType type,
    const std::vector<FilterHeader>& headers) const noexcept -> bool
{
    return StoreFilters(type, headers, {});
}

auto BlockFilter::StoreFilters(
    const FilterType type,
    std::vector<FilterData>& filters) const noexcept -> bool
{
    return StoreFilters(type, {}, filters);
}

auto BlockFilter::StoreFilters(
    const FilterType type,
    const std::vector<FilterHeader>& headers,
    const std::vector<FilterData>& filters) const noexcept -> bool
{
    auto parentTxn = lmdb_.TransactionRW();

    for (const auto& [block, header, hash] : headers) {
        auto proto = proto::BlockchainFilterHeader();
        proto.set_version(1);
        proto.set_header(header->str());
        proto.set_hash(std::string{hash});
        auto bytes = space(proto.ByteSize());
        proto.SerializeWithCachedSizesToArray(
            reinterpret_cast<std::uint8_t*>(bytes.data()));

        try {
            const auto stored = lmdb_.Store(
                translate_header(type),
                block->Bytes(),
                reader(bytes),
                parentTxn);

            if (false == stored.first) {
                LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save cfheaders")
                    .Flush();

                return false;
            }
        } catch (const std::exception& e) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();
        }
    }

    for (auto& [block, pFilter] : filters) {
        OT_ASSERT(pFilter);

        const auto& filter = *pFilter;

        try {
            const auto proto = [&] {
                auto out = proto::GCS{};

                if (false == filter.Serialize(out)) {
                    throw std::runtime_error{"Failed to serialize gcs"};
                }

                return out;
            }();
            auto bytes = space(proto.ByteSize());
            proto.SerializeWithCachedSizesToArray(
                reinterpret_cast<std::uint8_t*>(bytes.data()));
            const auto stored = lmdb_.Store(
                translate_filter(type), block, reader(bytes), parentTxn);

            if (false == stored.first) {
                LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save cfilters")
                    .Flush();

                return false;
            }
        } catch (const std::exception& e) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();
        }
    }

    return parentTxn.Finalize(true);
}

auto BlockFilter::translate_filter(const FilterType type) noexcept(false)
    -> Table
{
    switch (type) {
        case FilterType::Basic_BIP158: {
            return FiltersBasic;
        }
        case FilterType::Basic_BCHVariant: {
            return FiltersBCH;
        }
        case FilterType::ES: {
            return FiltersOpentxs;
        }
        default: {
            throw std::runtime_error("Unsupported filter type");
        }
    }
}

auto BlockFilter::translate_header(const FilterType type) noexcept(false)
    -> Table
{
    switch (type) {
        case FilterType::Basic_BIP158: {
            return FilterHeadersBasic;
        }
        case FilterType::Basic_BCHVariant: {
            return FilterHeadersBCH;
        }
        case FilterType::ES: {
            return FilterHeadersOpentxs;
        }
        default: {
            throw std::runtime_error("Unsupported filter type");
        }
    }
}
}  // namespace opentxs::blockchain::database::common
