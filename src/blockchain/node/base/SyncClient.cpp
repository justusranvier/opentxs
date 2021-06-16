// Copyright (c) 2010-2021 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"                         // IWYU pragma: associated
#include "1_Internal.hpp"                       // IWYU pragma: associated
#include "blockchain/node/base/SyncClient.hpp"  // IWYU pragma: associated

#include <zmq.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "api/network/blockchain/SyncClient.hpp"
#include "core/Worker.hpp"
#include "internal/api/network/Network.hpp"
#include "network/zeromq/socket/Socket.hpp"
#include "opentxs/Pimpl.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/LogSource.hpp"
#include "opentxs/network/blockchain/sync/Acknowledgement.hpp"
#include "opentxs/network/blockchain/sync/Base.hpp"
#include "opentxs/network/blockchain/sync/Data.hpp"
#include "opentxs/network/blockchain/sync/State.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/protobuf/BlockchainP2PChainState.pb.h"
#include "util/Backoff.hpp"
#include "util/ByteLiterals.hpp"

#define OT_METHOD "opentxs::blockchain::node::base::SyncClient::"

namespace sync = opentxs::network::blockchain::sync;

namespace opentxs::blockchain::node::base
{
struct SyncClient::Imp {
    enum class State { Init, Sync, Run };

    const std::string endpoint_;
    std::atomic<State> state_;

    Imp(const api::Core& api,
        const api::network::internal::Blockchain& network,
        const Type chain) noexcept
        : endpoint_(OTSocket::random_inproc_endpoint())
        , state_(State::Init)
        , api_(api)
        , chain_(chain)
        , limit_(std::max<Counter>(std::thread::hardware_concurrency(), 8))
        , lock_()
        , begin_sync_()
        , activity_()
        , dealer_([&] {
            auto out = Socket{
                ::zmq_socket(api_.Network().ZeroMQ(), ZMQ_DEALER), ::zmq_close};
            auto rc = ::zmq_setsockopt(
                out.get(), ZMQ_LINGER, &linger_, sizeof(linger_));

            OT_ASSERT(0 == rc);

            const auto& endpoint = network.SyncEndpoint();
            rc = ::zmq_connect(out.get(), endpoint.c_str());

            OT_ASSERT(0 == rc);

            LogTrace(OT_METHOD)(__FUNCTION__)(
                ": internal dealer connected to ")(endpoint)
                .Flush();

            return out;
        }())
        , subscribe_([&] {
            auto out = Socket{
                ::zmq_socket(api_.Network().ZeroMQ(), ZMQ_SUB), ::zmq_close};
            auto rc = ::zmq_setsockopt(
                out.get(), ZMQ_LINGER, &linger_, sizeof(linger_));

            OT_ASSERT(0 == rc);

            rc = ::zmq_setsockopt(out.get(), ZMQ_SUBSCRIBE, "", 0);

            OT_ASSERT(0 == rc);

            rc = ::zmq_connect(out.get(), api_.Endpoints().Shutdown().c_str());

            OT_ASSERT(0 == rc);

            return out;
        }())
        , pair_([&] {
            auto out = Socket{
                ::zmq_socket(api_.Network().ZeroMQ(), ZMQ_PAIR), ::zmq_close};
            auto rc = ::zmq_setsockopt(
                out.get(), ZMQ_LINGER, &linger_, sizeof(linger_));

            OT_ASSERT(0 == rc);

            rc = ::zmq_bind(out.get(), endpoint_.c_str());

            OT_ASSERT(0 == rc);

            return out;
        }())
        , local_position_(blank(api_))
        , remote_position_(blank(api_))
        , queue_position_(blank(api_))
        , timer_()
        , active_(0)
        , running_(true)
        , thread_(&Imp::thread, this)
    {
    }

    ~Imp()
    {
        running_ = false;

        if (thread_.joinable()) { thread_.join(); }
    }

private:
    using Socket = std::unique_ptr<void, decltype(&::zmq_close)>;
    using OTSocket = opentxs::network::zeromq::socket::implementation::Socket;
    using Task = api::network::blockchain::SyncClient::Task;
    using Counter = long long int;

    static constexpr int linger_{0};
    static constexpr std::chrono::seconds retry_interval_{4};

    const api::Core& api_;
    const Type chain_;
    const Counter limit_;
    mutable std::mutex lock_;
    Time begin_sync_;
    Time activity_;
    Socket dealer_;
    Socket subscribe_;
    Socket pair_;
    block::Position local_position_;
    block::Position remote_position_;
    block::Position queue_position_;
    Backoff timer_;
    Counter active_;
    std::atomic_bool running_;
    std::thread thread_;

    static auto blank(const api::Core& api) noexcept -> const block::Position&
    {
        static const auto output = block::Position{-1, api.Factory().Data()};

        return output;
    }

    auto blank() const noexcept -> const block::Position&
    {
        return blank(api_);
    }
    auto is_idle() const noexcept -> bool
    {
        if (local_position_ != remote_position_) { return false; }

        if (0 < active_) { return false; }

        return true;
    }
    auto next_position() const noexcept -> const block::Position&
    {
        return (blank() == queue_position_) ? local_position_ : queue_position_;
    }
    auto register_chain() const noexcept -> void
    {
        auto msg = MakeWork(api_, Task::Register);
        msg->AddFrame(chain_);
        LogDebug(OT_METHOD)(__FUNCTION__)(": registering ")(
            DisplayString(chain_))(" with high level api")
            .Flush();
        auto lock = Lock{lock_};
        OTSocket::send_message(lock, dealer_.get(), msg);
    }
    auto request(const block::Position& position) const noexcept -> void
    {
        if (blank() == position) { return; }

        auto msg = MakeWork(api_, Task::Request);
        msg->AddFrame(chain_);
        msg->AddFrame([&] {
            auto proto = proto::BlockchainP2PChainState{};
            const auto state = sync::State{chain_, position};
            state.Serialize(proto);

            return proto;
        }());
        LogVerbose(OT_METHOD)(__FUNCTION__)(": requesting sync data for ")(
            DisplayString(chain_))(" starting from block ")(position.first)
            .Flush();
        auto lock = Lock{lock_};
        OTSocket::send_message(lock, dealer_.get(), msg);
    }

    auto add_to_queue(OTZMQMessage&& msg) noexcept -> void
    {
        auto base = sync::Factory(api_, msg);
        auto& data = base->asData();
        const auto& blocks = data.Blocks();
        update_remote_position(data.State());

        if (0u == blocks.size()) { return; }

        update_queue_position(data);
        auto work = MakeWork(api_, WorkType::SyncReply);
        work->AddFrame(reinterpret_cast<std::uintptr_t>(base.release()));
        auto lock = Lock{lock_};

        if (false == OTSocket::send_message(lock, pair_.get(), work)) {
            OT_FAIL;
        }
    }
    auto do_init() noexcept -> void
    {
        if (timer_.test()) { register_chain(); }
    }
    auto do_run() noexcept -> void
    {
        do_sync();

        if (need_heartbeat()) { request(next_position()); }
    }
    auto do_sync() noexcept -> void
    {
        if (need_sync()) { request(next_position()); }
    }
    auto need_heartbeat() noexcept -> bool
    {
        if (false == is_idle()) { return false; }

        constexpr auto limit = std::chrono::minutes{2};
        const auto duration = Clock::now() - activity_;

        if (duration < limit) { return false; }

        if (timer_.test(retry_interval_)) {
            LogVerbose(OT_METHOD)(__FUNCTION__)(": more than ")(limit.count())(
                " minutes since data received for ")(DisplayString(chain_))
                .Flush();

            return true;
        } else {

            return false;
        }
    }
    auto need_sync() noexcept -> bool
    {
        if (local_position_ == remote_position_) {
            LogTrace(OT_METHOD)(__FUNCTION__)(": ")(DisplayString(chain_))(
                " is up to date")
                .Flush();

            return false;
        }

        if (queue_position_ == remote_position_) {
            LogTrace(OT_METHOD)(__FUNCTION__)(": ")(DisplayString(chain_))(
                " data already queued")
                .Flush();

            return false;
        }

        if (active_ >= limit_) {
            LogTrace(OT_METHOD)(__FUNCTION__)(": ")(DisplayString(chain_))(
                " pipeline is full with ")(active_)(" items ")
                .Flush();

            return false;
        }

        return timer_.test(retry_interval_);
    }
    auto process(void* socket) noexcept -> void
    {
        auto msg = [&] {
            auto output = api_.Network().ZeroMQ().Message();
            auto lock = Lock{lock_};
            OTSocket::receive_message(lock, socket, output);

            return output;
        }();

        if (0 == msg->size()) {
            LogTrace(OT_METHOD)(__FUNCTION__)(": Dropping empty message")
                .Flush();

            return;
        }

        try {
            const auto body = msg->Body();

            if (0 == body.size()) {
                throw std::runtime_error{"Empty message body"};
            }

            const auto task = body.at(0).as<Task>();

            switch (task) {
                case Task::Shutdown: {
                    running_ = false;
                } break;
                case Task::Ack: {
                    const auto base = sync::Factory(api_, msg);
                    const auto& ack = base->asAcknowledgement();
                    update_remote_position(ack.State(chain_));
                    activity_ = Clock::now();
                    LogVerbose(OT_METHOD)(__FUNCTION__)(
                        ": best chain tip for ")(DisplayString(chain_))(
                        " according to sync peer is ")(
                        remote_position_.second->asHex())(" at height ")(
                        remote_position_.first)
                        .Flush();

                    if (State::Init == state_.load()) {
                        timer_.reset();
                        begin_sync_ = Clock::now();
                        state_.store(State::Sync);
                    }
                } break;
                case Task::Reply: {
                    activity_ = Clock::now();
                    timer_.reset();
                    add_to_queue(std::move(msg));
                } break;
                case Task::Push: {
                    activity_ = Clock::now();

                    switch (state_.load()) {
                        case State::Run: {
                            add_to_queue(std::move(msg));
                        } break;
                        case State::Init:
                        case State::Sync:
                        default: {
                            LogDebug(OT_METHOD)(__FUNCTION__)(": ignoring ")(
                                DisplayString(chain_))(
                                " push notification until sync is complete")
                                .Flush();
                        }
                    }
                } break;
                case Task::Processed: {
                    OT_ASSERT(2 < body.size());

                    local_position_ = {
                        body.at(1).as<block::Height>(),
                        api_.Factory().Data(body.at(2).Bytes())};
                    --active_;
                } break;
                case Task::Server:
                case Task::Register:
                case Task::Request:
                default: {
                    OT_FAIL;
                }
            }
        } catch (const std::exception& e) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": ")(e.what()).Flush();

            return;
        }
    }
    auto state_machine() noexcept -> void
    {
        switch (state_.load()) {
            case State::Init: {
                do_init();
            } break;
            case State::Sync: {
                do_sync();

                if (is_idle()) {
                    const auto time =
                        std::chrono::duration_cast<std::chrono::seconds>(
                            Clock::now() - begin_sync_);
                    LogOutput(DisplayString(chain_))(" sync completed in ")(
                        time.count())(" seconds.")
                        .Flush();
                    state_.store(State::Run);
                }
            } break;
            case State::Run:
            default: {
                do_run();
            }
        }
    }
    auto thread() noexcept -> void
    {
        auto poll = [&] {
            auto output = std::array<::zmq_pollitem_t, 3>{};

            {
                auto& item = output.at(0);
                item.socket = dealer_.get();
                item.events = ZMQ_POLLIN;
            }
            {
                auto& item = output.at(1);
                item.socket = subscribe_.get();
                item.events = ZMQ_POLLIN;
            }
            {
                auto& item = output.at(2);
                item.socket = pair_.get();
                item.events = ZMQ_POLLIN;
            }

            return output;
        }();

        while (running_) {
            state_machine();
            constexpr auto timeout = std::chrono::milliseconds{1000};
            const auto events =
                ::zmq_poll(poll.data(), poll.size(), timeout.count());

            if (0 > events) {
                const auto error = ::zmq_errno();
                LogOutput(OT_METHOD)(__FUNCTION__)(": ")(::zmq_strerror(error))
                    .Flush();

                continue;
            } else if (0 == events) {

                continue;
            }

            for (auto& item : poll) {
                if (ZMQ_POLLIN != item.revents) { continue; }

                process(item.socket);
            }
        }
    }
    auto update_queue_position(const sync::Data& data) noexcept -> void
    {
        const auto& blocks = data.Blocks();

        if (0u == blocks.size()) { return; }

        queue_position_ = data.LastPosition(api_);
    }
    auto update_remote_position(const sync::State& state) noexcept -> void
    {
        remote_position_ = state.Position();
    }

    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;
};

SyncClient::SyncClient(
    const api::Core& api,
    const api::network::internal::Blockchain& network,
    const Type chain) noexcept
    : imp_(std::make_unique<Imp>(api, network, chain))
{
}

auto SyncClient::Endpoint() const noexcept -> const std::string&
{
    return imp_->endpoint_;
}

SyncClient::~SyncClient() = default;
}  // namespace opentxs::blockchain::node::base
