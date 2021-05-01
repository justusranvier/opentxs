// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "network/blockchain/bitmessage/PeerManager.hpp"  // IWYU pragma: associated

#include <chrono>
#include <future>

#include "core/Worker.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/util/WorkType.hpp"

// #define OT_METHOD "opentxs::network::blockchain::bitmessage::PeerManager::"

namespace zmq = opentxs::network::zeromq;

namespace opentxs::network::blockchain::bitmessage
{
using PeerWorker = Worker<PeerManager::Imp, api::Core>;

struct PeerManager::Imp final : public PeerWorker {
    friend PeerWorker;

    enum class Work : OTZMQWorkType {
        Shutdown = value(WorkType::Shutdown),
        Heartbeat = OT_ZMQ_HEARTBEAT_SIGNAL,
        StateMachine = OT_ZMQ_STATE_MACHINE_SIGNAL,
    };

    Imp(const api::Core& api)
        : Worker(api, std::chrono::milliseconds(100))
        , started_(false)
    {
    }

    ~Imp() { stop_worker().get(); }

private:
    bool started_;

    auto pipeline(zmq::Message& message) noexcept -> void
    {
        if (false == running_.get()) { return; }

        const auto body = message.Body();

        OT_ASSERT(0 < body.size());

        const auto work = [&] {
            try {

                return body.at(0).as<Work>();
            } catch (...) {

                OT_FAIL;
            }
        }();

        switch (work) {
            case Work::Shutdown: {
                shutdown(shutdown_promise_);
            } break;
            case Work::Heartbeat: {
                if (started_) { do_work(); }
            } break;
            case Work::StateMachine: {
                do_work();
            } break;
            default: {
                OT_FAIL;
            }
        }
    }
    auto shutdown(std::promise<void>& promise) noexcept -> void
    {
        if (running_->Off()) {
            try {
                promise.set_value();
            } catch (...) {
            }
        }
    }
    auto state_machine() noexcept -> bool
    {
        started_ = true;

        // FIXME

        return false;
    }
};

PeerManager::PeerManager(const api::Core& api) noexcept
    : imp_(std::make_unique<Imp>(api))
{
}

PeerManager::~PeerManager() = default;
}  // namespace opentxs::network::blockchain::bitmessage
