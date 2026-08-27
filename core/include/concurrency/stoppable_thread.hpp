#ifndef YODAU_CORE_CONCURRENCY_STOPPABLE_THREAD_HPP
#define YODAU_CORE_CONCURRENCY_STOPPABLE_THREAD_HPP

#include "core/namespace_alias.hpp"

#if defined(__ANDROID__)

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

namespace yodau::core {

// Android's NDK libc++ currently disables the C++20 stop-token API even
// though std::thread is available. Keep the public core API portable with a
// small cooperative-cancellation adapter that has the subset of jthread
// semantics used by yodau.
class stop_token {
public:
    stop_token() = default;

    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ && state_->load(std::memory_order_acquire);
    }

    [[nodiscard]] bool stop_possible() const noexcept {
        return static_cast<bool>(state_);
    }

private:
    explicit stop_token(std::shared_ptr<std::atomic_bool> state)
        : state_(std::move(state)) { }

    std::shared_ptr<std::atomic_bool> state_;

    friend class stop_source;
};

class stop_source {
public:
    stop_source()
        : state_(std::make_shared<std::atomic_bool>(false)) { }

    [[nodiscard]] stop_token get_token() const noexcept {
        return stop_token(state_);
    }

    bool request_stop() noexcept {
        return state_ && !state_->exchange(true, std::memory_order_acq_rel);
    }

private:
    std::shared_ptr<std::atomic_bool> state_;
};

class stoppable_thread {
public:
    stoppable_thread() = default;

    template <typename Function, typename... Args>
        requires(
            !std::is_same_v<std::remove_cvref_t<Function>, stoppable_thread>
        )
    explicit stoppable_thread(Function&& function, Args&&... args) {
        const stop_token token = stop_source_.get_token();
        if constexpr (
            std::is_invocable_v<
                std::decay_t<Function>, stop_token, std::decay_t<Args>...>
        ) {
            thread_ = std::thread(
                std::forward<Function>(function), token,
                std::forward<Args>(args)...
            );
        } else {
            thread_ = std::thread(
                std::forward<Function>(function), std::forward<Args>(args)...
            );
        }
    }

    ~stoppable_thread() {
        if (joinable()) {
            request_stop();
            join();
        }
    }

    stoppable_thread(const stoppable_thread&) = delete;
    stoppable_thread& operator=(const stoppable_thread&) = delete;

    stoppable_thread(stoppable_thread&& other) noexcept
        : thread_(std::move(other.thread_))
        , stop_source_(std::move(other.stop_source_)) { }

    stoppable_thread& operator=(stoppable_thread&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (joinable()) {
            request_stop();
            join();
        }
        thread_ = std::move(other.thread_);
        stop_source_ = std::move(other.stop_source_);
        return *this;
    }

    [[nodiscard]] bool joinable() const noexcept { return thread_.joinable(); }

    void join() { thread_.join(); }

    bool request_stop() noexcept { return stop_source_.request_stop(); }

    [[nodiscard]] stop_token get_stop_token() const noexcept {
        return stop_source_.get_token();
    }

private:
    std::thread thread_;
    stop_source stop_source_;
};

} // namespace yodau::core

#else

#include <stop_token>
#include <thread>

namespace yodau::core {

using stop_token = std::stop_token;
using stop_source = std::stop_source;
using stoppable_thread = std::jthread;

} // namespace yodau::core

#endif

#endif // YODAU_CORE_CONCURRENCY_STOPPABLE_THREAD_HPP
