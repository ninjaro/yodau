#include "analysis/processing_runtime.hpp"
#include "monitor/client.hpp"
#include "monitor/platform/shared_memory.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <iostream>
#include <limits>
#include <semaphore>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <thread>

namespace {

class runtime_monitoring_test : public testing::Test {
protected:
    void SetUp() override {
        if constexpr (monitor::instrumentation_enabled) {
            ASSERT_TRUE(monitor::start_process("yodau-mon-core-test"));
            mapping = monitor::platform::open_process_mapping(
                monitor::platform::current_process_id()
            );
            ASSERT_TRUE(mapping.has_value());
            auto* state = mapping->get();
            state->watcher_lease_until_ns.store(
                std::numeric_limits<std::uint64_t>::max()
            );
            state->watcher_present.store(1);
        }
    }

    void TearDown() override { monitor::client::process().stop(); }

    [[nodiscard]] monitor::heartbeat_slot& core() const {
        return mapping->get()
            ->heartbeats[monitor::channel_index(monitor::channel::core)];
    }

    static yodau::core::frame frame() {
        yodau::core::frame value;
        value.width = 64;
        value.height = 48;
        value.stride = 64 * 3;
        value.format = yodau::core::pixel_format::bgr24;
        value.data.resize(64U * 48U * 3U);
        return value;
    }

    std::optional<monitor::platform::shared_mapping> mapping;
    const yodau::core::stream source { "unused.mp4", "source", "file", false };
};

// Stops at a real host callback boundary, after algorithm completion. No clock
// sleeps or physical capture devices are needed to inspect a stalled callback.
class blocked_frame {
public:
    explicit blocked_frame(
        yodau::core::processing_runtime& runtime,
        const yodau::core::stream& source, const yodau::core::frame& frame
    ) {
        runtime.set_processed_frame_observer(
            [this](const auto&, const auto&, const auto&) {
                entered_.set_value();
                resume_.acquire();
            }
        );
        worker_ = std::jthread([callback = runtime.frame_processor_hook(),
                                &source, &frame] {
            static_cast<void>(callback(source, frame));
        });
    }

    ~blocked_frame() { finish(); }

    blocked_frame(const blocked_frame&) = delete;
    blocked_frame& operator=(const blocked_frame&) = delete;

    bool entered() {
        return entered_.get_future().wait_for(std::chrono::seconds(2))
            == std::future_status::ready;
    }

    void finish() {
        if (worker_.joinable()) {
            resume_.release();
            worker_.join();
        }
    }

private:
    std::promise<void> entered_;
    std::binary_semaphore resume_ { 0 };
    std::jthread worker_;
};

class captured_monitoring_warning {
public:
    captured_monitoring_warning()
        : previous_(std::clog.rdbuf(text.rdbuf())) { }

    ~captured_monitoring_warning() { std::clog.rdbuf(previous_); }

    captured_monitoring_warning(const captured_monitoring_warning&) = delete;
    captured_monitoring_warning& operator=(const captured_monitoring_warning&)
        = delete;
    std::ostringstream text;

private:
    std::streambuf* previous_;
};

TEST_F(
    runtime_monitoring_test, idle_and_callback_completion_retire_expectation
) {
    if constexpr (!monitor::instrumentation_enabled) {
        EXPECT_FALSE(monitor::process_available());
        yodau::core::processing_runtime runtime;
        const auto callback = runtime.frame_processor_hook();
        ASSERT_TRUE(static_cast<bool>(callback));
        static_cast<void>(callback(source, frame()));
        EXPECT_FALSE(monitor::process_available());
        return;
    }
    yodau::core::processing_runtime runtime;
    ASSERT_TRUE(runtime.processing_enabled());
    EXPECT_EQ(core().active.load(), 0U);
    const auto input = frame();
    blocked_frame work(runtime, source, input);
    ASSERT_TRUE(work.entered());
    EXPECT_EQ(core().active.load(), 1U);
    EXPECT_GT(core().expected_since_ns.load(), 0U);
    const auto timestamp = core().timestamp_ns.load();
    const auto sequence = core().sequence.load();
    const auto expected = core().expected_since_ns.load();
    const auto revision = core().revision.load();
    EXPECT_EQ(timestamp, 0U);
    EXPECT_EQ(sequence, 0U);
    // A manual pulse cannot disguise the stalled observer callback.
    monitor::heartbeat(monitor::channel::core);
    monitor::set_channel_active(monitor::channel::core, true);
    monitor::set_channel_active(monitor::channel::core, false);
    monitor::record_breadcrumb(monitor::event::stream_started);
    auto supervisor = monitor::expect_progress(monitor::channel::main);
    ASSERT_TRUE(supervisor.valid());
    supervisor.heartbeat();
    EXPECT_GT(
        mapping->get()
            ->heartbeats[monitor::channel_index(monitor::channel::main)]
            .timestamp_ns.load(),
        0U
    );
    auto other = monitor::expect_progress(monitor::channel::worker);
    ASSERT_TRUE(other.valid());
    other.heartbeat();
    EXPECT_GT(
        mapping->get()
            ->heartbeats[monitor::channel_index(monitor::channel::worker)]
            .timestamp_ns.load(),
        0U
    );
    EXPECT_EQ(core().timestamp_ns.load(), timestamp);
    EXPECT_EQ(core().sequence.load(), sequence);
    EXPECT_EQ(core().expected_since_ns.load(), expected);
    EXPECT_EQ(core().revision.load(), revision);
    work.finish();
    EXPECT_EQ(core().active.load(), 0U);
    EXPECT_EQ(core().expected_since_ns.load(), 0U);
}

TEST_F(runtime_monitoring_test, overlap_cannot_publish_for_or_retire_an_owner) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    yodau::core::processing_runtime first;
    yodau::core::processing_runtime second;
    yodau::core::processing_runtime replacement;
    const auto input = frame();
    blocked_frame owned(first, source, input);
    ASSERT_TRUE(owned.entered());
    const auto expected = core().expected_since_ns.load();
    const auto revision = core().revision.load();
    captured_monitoring_warning warning;
    blocked_frame unmonitored(second, source, input);
    ASSERT_TRUE(unmonitored.entered());
    EXPECT_NE(
        warning.text.str().find("overlapping frame callback"), std::string::npos
    );
    EXPECT_EQ(core().expected_since_ns.load(), expected);
    EXPECT_EQ(core().revision.load(), revision);
    // The second operation is explicitly unsupported, not an implicit co-owner.
    owned.finish();
    EXPECT_EQ(core().active.load(), 0U);
    blocked_frame current(replacement, source, input);
    ASSERT_TRUE(current.entered());
    const auto current_revision = core().revision.load();
    unmonitored.finish();
    EXPECT_EQ(core().active.load(), 1U);
    EXPECT_EQ(core().revision.load(), current_revision);
    current.finish();
    EXPECT_EQ(core().active.load(), 0U);
}

TEST_F(runtime_monitoring_test, another_stream_cannot_mask_a_stalled_callback) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    yodau::core::processing_runtime runtime;
    const auto input = frame();
    const yodau::core::stream other_source(
        "unused-other.mp4", "other", "file", false
    );
    std::promise<void> entered;
    std::binary_semaphore resume(0);
    runtime.set_processed_frame_observer([&](const auto& observed, const auto&,
                                             const auto&) {
        if (observed.get_name() == source.get_name()) {
            entered.set_value();
            resume.acquire();
        }
    });
    const auto callback = runtime.frame_processor_hook();
    std::jthread stalled([&] { static_cast<void>(callback(source, input)); });
    entered.get_future().wait();
    const auto revision = core().revision.load();
    captured_monitoring_warning warning;
    for (int frame_index = 0; frame_index < 8; ++frame_index) {
        static_cast<void>(callback(other_source, input));
        EXPECT_EQ(core().revision.load(), revision);
    }
    const auto diagnostic = warning.text.str();
    const auto first = diagnostic.find("overlapping frame callback");
    EXPECT_NE(first, std::string::npos);
    EXPECT_EQ(
        diagnostic.find("overlapping frame callback", first + 1),
        std::string::npos
    );
    resume.release();
    stalled.join();
    EXPECT_EQ(core().active.load(), 0U);
}

TEST_F(runtime_monitoring_test, detached_callback_cannot_advance_replacement) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    yodau::core::processing_runtime runtime;
    auto stale = runtime.frame_processor_hook();
    runtime.detach();
    const auto input = frame();
    blocked_frame current(runtime, source, input);
    ASSERT_TRUE(current.entered());
    const auto revision = core().revision.load();
    EXPECT_TRUE(stale(source, input).empty());
    EXPECT_EQ(core().revision.load(), revision);
    current.finish();
    runtime.detach();
    EXPECT_EQ(core().active.load(), 0U);
}

TEST_F(runtime_monitoring_test, detach_joins_executing_callback_before_reuse) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    yodau::core::processing_runtime runtime;
    const auto stale = runtime.frame_processor_hook();
    const auto input = frame();
    blocked_frame work(runtime, source, input);
    ASSERT_TRUE(work.entered());
    std::promise<void> detaching;
    auto started = detaching.get_future();
    auto detached = std::async(std::launch::async, [&] {
        detaching.set_value();
        runtime.detach();
    });
    started.wait();
    // Cancellation must not erase evidence of work still executing. Detach
    // retains its existing join contract, rather than pretending to interrupt
    // it.
    EXPECT_EQ(core().active.load(), 1U);
    work.finish();
    detached.get();
    EXPECT_EQ(core().active.load(), 0U);
    blocked_frame current(runtime, source, input);
    ASSERT_TRUE(current.entered());
    const auto revision = core().revision.load();
    EXPECT_TRUE(stale(source, input).empty());
    EXPECT_EQ(core().revision.load(), revision);
}

TEST_F(runtime_monitoring_test, observer_exception_retires_expectation) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    yodau::core::processing_runtime runtime;
    runtime.set_processed_frame_observer([](const auto&, const auto&,
                                            const auto&) {
        throw std::runtime_error("observer failed");
    });
    const auto callback = runtime.frame_processor_hook();
    EXPECT_THROW(callback(source, frame()), std::runtime_error);
    EXPECT_EQ(core().active.load(), 0U);
    EXPECT_EQ(core().expected_since_ns.load(), 0U);
}

TEST_F(runtime_monitoring_test, eof_and_cancelled_input_wait_leave_core_idle) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    yodau::core::processing_runtime runtime;
    const auto input = frame();
    for (const bool cancel : { false, true }) {
        yodau::core::stream_manager manager;
        manager.add_stream("unused.mp4", "source", "file", false);
        runtime.attach(manager);
        std::promise<void> delivered;
        auto ready = delivered.get_future();
        manager.set_daemon_start_hook([&](const auto&, const auto& push,
                                          const std::stop_token& stop) {
            push(yodau::core::frame(input));
            delivered.set_value();
            if (cancel) {
                std::mutex mutex;
                std::condition_variable_any done;
                std::unique_lock lock(mutex);
                done.wait(lock, stop, [] { return false; });
            }
        });
        manager.start_stream("source");
        ASSERT_EQ(
            ready.wait_for(std::chrono::seconds(2)), std::future_status::ready
        );
        EXPECT_EQ(core().active.load(), 0U);
        manager.stop_stream("source");
        manager.shutdown();
        runtime.detach();
        EXPECT_EQ(core().active.load(), 0U);
    }
}

TEST_F(runtime_monitoring_test, stop_request_is_not_executing_work_completion) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    const auto input = frame();
    std::promise<void> entered;
    std::promise<void> cancellation;
    auto entry = entered.get_future();
    auto cancelled = cancellation.get_future();
    yodau::core::processing_runtime runtime;
    yodau::core::stream_manager manager;

    struct completion_gate {
        yodau::core::stream_manager& host;
        std::binary_semaphore resume { 0 };

        ~completion_gate() {
            resume.release();
            host.stop_stream("source");
        }
    } gate { manager };

    // Release and join before destroying the semaphore on an early assertion;
    // the promises and runtime outlive the manager's worker in either path.
    runtime.attach(manager);
    runtime.set_processed_frame_observer([&](const auto&, const auto&,
                                             const auto&) {
        entered.set_value();
        gate.resume.acquire();
    });
    manager.add_stream("unused.mp4", "source", "file", false);
    manager.set_daemon_start_hook([&](const auto&, const auto& push,
                                      const std::stop_token& stop) {
        const std::stop_callback observed(stop, [&] {
            cancellation.set_value();
        });
        push(yodau::core::frame(input));
    });
    manager.start_stream("source");
    ASSERT_EQ(
        entry.wait_for(std::chrono::seconds(2)), std::future_status::ready
    );
    const auto expected = core().expected_since_ns.load();
    auto stopped = std::async(std::launch::async, [&] {
        manager.stop_stream("source");
    });
    EXPECT_EQ(
        cancelled.wait_for(std::chrono::seconds(2)), std::future_status::ready
    );
    EXPECT_EQ(core().active.load(), 1U);
    EXPECT_EQ(core().expected_since_ns.load(), expected);
    EXPECT_EQ(core().sequence.load(), 0U);
    gate.resume.release();
    stopped.get();
    EXPECT_EQ(core().active.load(), 0U);
    EXPECT_EQ(core().expected_since_ns.load(), 0U);
    manager.shutdown();
    runtime.detach();
}

} // namespace
