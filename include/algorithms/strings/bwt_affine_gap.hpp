#pragma once

#include "algorithms/strings/bwt_index.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct AffineGapCosts {
  std::size_t substitution = 1U;
  std::size_t gap_open = 1U;
  std::size_t gap_extend = 1U;
};

struct AffineGapBwtSearchResult {
  std::vector<std::size_t> positions;
  std::size_t expanded_states = 0U;
  std::size_t transitions_considered = 0U;
  std::size_t pruned_empty_transitions = 0U;
  std::size_t pruned_over_budget_transitions = 0U;
  std::size_t dominated_states = 0U;
  std::size_t terminal_states = 0U;
  std::size_t peak_frontier_size = 0U;
};

// Exact substring-start search under a bounded byte-oriented affine-gap score.
// A contiguous insertion/deletion gap of length L>=1 costs
// gap_open + (L-1)*gap_extend. All three non-match costs must be positive.
// Search state remains entirely on the sealed bidirectional BWT surface; the
// implementation does not scan source text directly.
[[nodiscard]] AffineGapBwtSearchResult locate_bwt_affine_gap(
    const BidirectionalBwtByteIndex& index, std::string_view pattern,
    std::size_t max_score, AffineGapCosts costs = {});

}  // namespace algorithms::strings
