#pragma once

#include "algorithms/strings/bwt_index.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct CoalescedSeedVerifyEditDistanceResult {
  std::vector<std::size_t> positions;
  std::size_t seed_count = 0U;
  std::size_t seed_occurrences = 0U;
  std::size_t unique_candidates = 0U;
  std::size_t verified_candidates = 0U;
  std::size_t verification_dp_cells = 0U;
  std::size_t candidate_windows = 0U;
  std::size_t merged_extraction_windows = 0U;
  std::size_t extracted_bytes = 0U;
  std::size_t extraction_lf_steps = 0U;
};

// Exact bounded-Levenshtein substring-start search. Candidate generation is
// identical to the sealed Phase-32 implementation, but overlapping or touching
// candidate verification windows are coalesced and extracted once per merged
// interval through the sealed Phase-31 periodic-sample extractor.
[[nodiscard]] CoalescedSeedVerifyEditDistanceResult
locate_bwt_coalesced_seed_verify_edit_distance(
    const BwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits);

}  // namespace algorithms::strings
