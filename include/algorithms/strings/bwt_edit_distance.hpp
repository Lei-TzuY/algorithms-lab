#pragma once

#include "algorithms/strings/bwt_index.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct EditDistanceBwtSearchResult {
  std::vector<std::size_t> positions;
  std::size_t expanded_states = 0U;
  std::size_t transitions_considered = 0U;
  std::size_t pruned_empty_transitions = 0U;
  std::size_t dominated_states = 0U;
  std::size_t terminal_states = 0U;
  std::size_t peak_frontier_size = 0U;
};

// Exact substring-start search under a bounded byte-oriented Levenshtein
// budget. Search state is carried entirely by the sealed bidirectional BWT
// index; the implementation does not scan the source text directly.
[[nodiscard]] EditDistanceBwtSearchResult locate_bwt_edit_distance(
    const BidirectionalBwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits);

}  // namespace algorithms::strings
