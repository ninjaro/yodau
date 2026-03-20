#ifndef YODAU_BACKEND_DEFAULT_PROCESSING_HOOKS_HPP
#define YODAU_BACKEND_DEFAULT_PROCESSING_HOOKS_HPP

#include "streams/stream_manager.hpp"

namespace yodau::backend {

stream_manager::daemon_start_fn default_daemon_start_hook();
stream_manager::frame_processor_fn default_frame_processor();

} // namespace yodau::backend

#endif // YODAU_BACKEND_DEFAULT_PROCESSING_HOOKS_HPP
