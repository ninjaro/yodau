#ifndef YODAU_BACKEND_ANALYSIS_PROCESSING_MOTION_REGION_FILTER_HPP
#define YODAU_BACKEND_ANALYSIS_PROCESSING_MOTION_REGION_FILTER_HPP

#include "analysis/processing_algorithm.hpp"
#include "streams/stream.hpp"

namespace yodau::backend {

class processing_motion_region_filter {
public:
    processing_result
    apply(const stream& stream_value, processing_result result) const;
};

} // namespace yodau::backend

#endif // YODAU_BACKEND_ANALYSIS_PROCESSING_MOTION_REGION_FILTER_HPP
