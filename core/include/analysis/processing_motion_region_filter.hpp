#ifndef YODAU_CORE_ANALYSIS_PROCESSING_MOTION_REGION_FILTER_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_MOTION_REGION_FILTER_HPP

#include "core/namespace_alias.hpp"
#include "analysis/processing_algorithm.hpp"
#include "streams/stream.hpp"

namespace yodau::core {

class processing_motion_region_filter {
public:
    processing_result
    apply(const stream& stream_value, processing_result result) const;
};

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_MOTION_REGION_FILTER_HPP
