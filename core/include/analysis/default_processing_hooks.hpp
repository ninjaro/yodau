#ifndef YODAU_CORE_DEFAULT_PROCESSING_HOOKS_HPP
#define YODAU_CORE_DEFAULT_PROCESSING_HOOKS_HPP

#include "analysis/processing_algorithm.hpp"
#include "streams/stream_manager.hpp"

#include <memory>
#include <string>

namespace yodau::core {

stream_manager::daemon_start_fn default_daemon_start_hook();
stream_manager::frame_processor_fn default_frame_processor();
std::string default_processing_algorithm_id();
const processing_algorithm_registry& default_processing_algorithm_registry();
std::unique_ptr<processing_algorithm>
make_processing_algorithm(const std::string& algorithm_id = {});

} // namespace yodau::core

#endif // YODAU_CORE_DEFAULT_PROCESSING_HOOKS_HPP
