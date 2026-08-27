#ifndef YODAU_CORE_ANALYSIS_DEFAULT_PROCESSING_ALGORITHMS_HPP
#define YODAU_CORE_ANALYSIS_DEFAULT_PROCESSING_ALGORITHMS_HPP

#include "analysis/processing_algorithm.hpp"

#include <memory>

namespace yodau::core {

std::unique_ptr<processing_algorithm> make_motion_baseline_algorithm();
std::unique_ptr<processing_algorithm> make_portable_motion_baseline_algorithm();

#ifdef YODAU_OPENCV
std::unique_ptr<processing_algorithm> make_spot_grid_algorithm();
std::unique_ptr<processing_algorithm> make_contour_mask_algorithm();
std::unique_ptr<processing_algorithm> make_centroid_track_algorithm();
std::unique_ptr<processing_algorithm> make_hybrid_auto_algorithm();
#endif

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_DEFAULT_PROCESSING_ALGORITHMS_HPP
