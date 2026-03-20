#include "analysis/default_processing_hooks.hpp"

#include "analysis/opencv_client.hpp"

namespace yodau::backend {

stream_manager::daemon_start_fn default_daemon_start_hook() {
#ifdef YODAU_OPENCV
    return opencv_daemon_start;
#else
    return {};
#endif
}

stream_manager::frame_processor_fn default_frame_processor() {
#ifdef YODAU_OPENCV
    return opencv_motion_processor;
#else
    return {};
#endif
}

} // namespace yodau::backend
