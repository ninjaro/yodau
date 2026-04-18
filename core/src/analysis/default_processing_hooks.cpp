#include "analysis/default_processing_hooks.hpp"

#include "analysis/default_processing_algorithms.hpp"
#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_algorithm_ids.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace yodau::core {

namespace default_processing_hooks_support {

using namespace processing_algorithm_ids;

std::string normalized_requested_algorithm_id(const std::string& algorithm_id) {
    return normalized_processing_algorithm_id(algorithm_id);
}

processing_algorithm_registry::entry registry_entry(
    const std::string_view algorithm_id,
    std::function<std::unique_ptr<processing_algorithm>()> create
) {
    const std::string normalized_algorithm_id
        = normalized_processing_algorithm_id(algorithm_id);
    return processing_algorithm_registry::entry {
        .algorithm_id = normalized_algorithm_id,
        .display_name = processing_algorithm_display_name(
            normalized_algorithm_id
        ),
        .create = std::move(create),
    };
}

const processing_algorithm_registry& registry_instance() {
    static const processing_algorithm_registry registry = [] {
        processing_algorithm_registry value;
#ifdef YODAU_OPENCV
        value.register_algorithm(
            registry_entry(
                motion_baseline,
                [] {
                    return make_motion_baseline_algorithm();
                }
            )
        );
        value.register_algorithm(
            registry_entry(
                spot_grid,
                [] {
                    return make_spot_grid_algorithm();
                }
            )
        );
        value.register_algorithm(
            registry_entry(
                contour_mask,
                [] {
                    return make_contour_mask_algorithm();
                }
            )
        );
        value.register_algorithm(
            registry_entry(
                centroid_track,
                [] {
                    return make_centroid_track_algorithm();
                }
            )
        );
        value.register_algorithm(
            registry_entry(
                hybrid_auto,
                [] {
                    return make_hybrid_auto_algorithm();
                }
            )
        );
#endif
        return value;
    }();

    return registry;
}

processing_algorithm* shared_default_algorithm_instance() {
    static std::unique_ptr<processing_algorithm> algorithm
        = make_processing_algorithm();
    return algorithm.get();
}

} // namespace default_processing_hooks_support

stream_manager::daemon_start_fn default_daemon_start_hook() {
    if (default_processing_hooks_support::shared_default_algorithm_instance()
        == nullptr) {
        return {};
    }

    return [](
               const stream& stream_value,
               const std::function<void(frame&&)>& on_frame,
               const std::stop_token& stop_token
           ) {
        if (auto* algorithm
            = default_processing_hooks_support::shared_default_algorithm_instance()) {
            algorithm->daemon_start(stream_value, on_frame, stop_token);
        }
    };
}

stream_manager::frame_processor_fn default_frame_processor() {
    if (default_processing_hooks_support::shared_default_algorithm_instance()
        == nullptr) {
        return {};
    }

    return [](const stream& stream_value, const frame& frame_value) {
        if (auto* algorithm
            = default_processing_hooks_support::shared_default_algorithm_instance()) {
            return algorithm->process_frame(stream_value, frame_value).events;
        }

        return std::vector<event> {};
    };
}

std::string default_processing_algorithm_id() {
    const std::string candidate = normalized_processing_algorithm_id(
        processing_algorithm_ids::motion_baseline
    );
    return default_processing_algorithm_registry().contains(candidate) ? candidate
                                                                       : std::string {};
}

const processing_algorithm_registry& default_processing_algorithm_registry() {
    return default_processing_hooks_support::registry_instance();
}

std::unique_ptr<processing_algorithm>
make_processing_algorithm(const std::string& algorithm_id) {
    const auto& registry = default_processing_algorithm_registry();
    const std::string requested_algorithm_id
        = default_processing_hooks_support::normalized_requested_algorithm_id(
            algorithm_id
        );

    if (!requested_algorithm_id.empty()) {
        if (auto algorithm = registry.create(requested_algorithm_id)) {
            return algorithm;
        }
    }

    const std::string fallback_algorithm_id = default_processing_algorithm_id();
    if (fallback_algorithm_id.empty()) {
        return {};
    }

    return registry.create(fallback_algorithm_id);
}

} // namespace yodau::core
