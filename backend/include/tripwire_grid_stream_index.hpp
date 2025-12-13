#ifndef YODAU_BACKEND_TRIPWIRE_GRID_STREAM_INDEX_HPP
#define YODAU_BACKEND_TRIPWIRE_GRID_STREAM_INDEX_HPP

#include "tripwire_grid_index.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace yodau::backend {

struct grid_compiled_line {
    std::string name;
    tripwire_dir dir { tripwire_dir::any };
    grid_line_index index;
};

struct grid_segment_ref {
    std::size_t id {};
    std::size_t line_index {};
    std::size_t seg_index {};
};

struct grid_stream_index {
    grid_dims dims;
    std::vector<grid_compiled_line> lines;
    std::vector<grid_segment_ref> segments;
    std::vector<std::vector<std::size_t>> cell_to_segment_ids;
};

struct grid_candidate_tracker {
    std::vector<std::uint32_t> seen;
    std::uint32_t stamp { 1 };

    void ensure_size(const std::size_t n);

    void next_stamp();
};

grid_stream_index build_grid_stream_index(
    const std::vector<line_ptr>& input_lines, const grid_dims& g
);

void collect_grid_candidates(
    const grid_stream_index& idx, const std::vector<int>& active_cell_indices,
    grid_candidate_tracker& tracker, std::vector<std::size_t>& out_segment_ids
);
}

#endif // YODAU_BACKEND_TRIPWIRE_GRID_STREAM_INDEX_HPP