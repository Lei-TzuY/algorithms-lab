#pragma once

#include "algorithms/strings/bwt_index.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct CandidateLocalSeedVerifyEditDistanceResult {
  std::vector<std::size_t> positions;
  std::size_t seed_count = 0U;
  std::size_t seed_occurrences = 0U;
  std::size_t unique_candidates = 0U;
  std::size_t verified_candidates = 0U;
  std::size_t verification_dp_cells = 0U;
  std::size_t extraction_windows = 0U;
  std::size_t extracted_bytes = 0U;
  std::size_t extraction_lf_steps = 0U;
};

// Exact substring-start search under bounded byte-oriented Levenshtein distance.
// Candidate generation preserves the sealed Phase-30 k+1 exact-seed
// completeness argument. Each surviving candidate is verified from a bounded
// source window reconstructed through the sealed Phase-31 periodic-sample
// extractor rather than through unconditional full-text reconstruction.
[[nodiscard]] CandidateLocalSeedVerifyEditDistanceResult
locate_bwt_candidate_local_seed_verify_edit_distance(
    const BwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits);

}  // namespace algorithms::strings
