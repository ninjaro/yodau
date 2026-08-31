#ifndef YODAU_CORE_ANALYSIS_PROCESSING_PREVIEW_ROUTER_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_PREVIEW_ROUTER_HPP

#include "analysis/processing_algorithm.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"

#include <memory>
#include <string>
#include <vector>

namespace yodau::core {

class virtual_camera;

class processing_preview_router {
public:
    explicit processing_preview_router(
        bool enable_virtual_camera, std::string virtual_camera_device = {}
    );
    ~processing_preview_router();

    processing_preview_router(const processing_preview_router&) = delete;
    processing_preview_router& operator=(const processing_preview_router&)
        = delete;
    processing_preview_router(processing_preview_router&&) noexcept;
    processing_preview_router& operator=(processing_preview_router&&) noexcept;

    [[nodiscard]] bool has_virtual_camera() const;
    [[nodiscard]] virtual_camera* preview_camera();
    [[nodiscard]] const virtual_camera* preview_camera() const;

    void publish_processed_frame(
        const stream& stream_value, const frame& source_frame,
        const std::vector<event>& events,
        const processing_result* latest_result = nullptr
    );

private:
    std::unique_ptr<virtual_camera> preview_camera_value;
};

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_PREVIEW_ROUTER_HPP
