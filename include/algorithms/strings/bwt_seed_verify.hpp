#pragma once

#include "algorithms/strings/bwt_index.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct SeedVerifyEditDistanceResult {
  std::vector<std::size_t> positions;
  std::size_t seed_count = 0U;
  std::size_t seed_occurrences = 0U;
  std::size_t unique_candidates = 0U;
  std::size_t verified_candidates = 0U;
  std::size_t verification_dp_cells = 0U;
  std::size_t reconstruction_lf_steps = 0U;
};

// Exact substring-start search under bounded byte-oriented Levenshtein distance.
// Candidate generation uses k+1 non-overlapping exact BWT seeds; candidates are
// verified against one exact Phase-29 text reconstruction. Finite candidate
// filtering is a completeness-preserving algorithm, not a heuristic seed rule.
[[nodiscard]] SeedVerifyEditDistanceResult locate_bwt_seed_verify_edit_distance(
    const BwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits);

}  // namespace algorithms::strings
