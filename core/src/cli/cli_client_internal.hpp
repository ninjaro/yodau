#pragma once

#include "analysis/processing_algorithm.hpp"
#include "geometry/geometry.hpp"

#include <stdexcept>
#include <string>

namespace yodau::core::cli_detail {

inline std::string bool_text(const bool value) {
    return value ? "true" : "false";
}

/// Parse a CLI direction, rejecting unknown tokens.
inline tripwire_dir tripwire_direction(const std::string& text) {
    if (text == "neg_to_pos") {
        return tripwire_dir::neg_to_pos;
    }
    if (text == "pos_to_neg") {
        return tripwire_dir::pos_to_neg;
    }
    if (text == "any") {
        return tripwire_dir::any;
    }
    throw std::runtime_error("unknown dir: " + text);
}

/// Resolve command aliases to the registry's normalized algorithm id.
inline std::string algorithm_id(const std::string& text) {
    return processing_algorithm_registry::normalized_algorithm_id(text);
}

} // namespace yodau::core::cli_detail
