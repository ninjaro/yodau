#ifndef YODAU_CORE_OPENCV_CLIENT_HPP
#define YODAU_CORE_OPENCV_CLIENT_HPP

#ifdef YODAU_OPENCV

#include "analysis/processing_motion_tools.hpp"
#include "analysis/tripwire_grid_stream_index.hpp"
#include "core/namespace_alias.hpp"
#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"
#include "streams/stream_manager.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::core {

class opencv_client {
public:
    opencv_client() = default;

    static void daemon_start(
        const stream& s, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& st
    );

    std::vector<event> motion_processor(const stream& s, const frame& f);

    stream_manager::daemon_start_fn daemon_start_fn();
    stream_manager::frame_processor_fn frame_processor_fn();
    static opencv_client& shared_instance();

private:
    static bool is_null_line_ptr(const line_ptr& line_ptr_value);
    static bool line_ptr_less_by_name(const line_ptr& a, const line_ptr& b);
    static void normalize_lines_snapshot(std::vector<line_ptr>& lines);

    struct grid_cache_entry {
        grid_dims dims;
        std::vector<line_ptr> line_snapshots;
        std::shared_ptr<const grid_stream_index> index;
        size_t reuse_count { 0 };
        std::uint64_t generation { 0 };
    };

    std::shared_ptr<const grid_stream_index>
    get_grid_index_cached(const stream& s, const std::vector<line_ptr>& lines);

    processing_motion_event_state motion_state_;
    mutable std::mutex grid_cache_mtx;
    std::unordered_map<std::string, grid_cache_entry> grid_cache_by_stream;
};

void opencv_daemon_start(
    const stream& s, const std::function<void(frame&&)>& on_frame,
    const std::stop_token& st
);

std::vector<event> opencv_motion_processor(const stream& s, const frame& f);

}

#endif
#endif // YODAU_CORE_OPENCV_CLIENT_HPP
