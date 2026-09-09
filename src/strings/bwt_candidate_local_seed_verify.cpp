#include "algorithms/strings/bwt_candidate_local_seed_verify.hpp"

#include "bwt_seed_verify_common.hpp"

#include <cstdint>
#include <string_view>

namespace algorithms::strings {

CandidateLocalSeedVerifyEditDistanceResult
locate_bwt_candidate_local_seed_verify_edit_distance(
    const BwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits) {
  CandidateLocalSeedVerifyEditDistanceResult result;
  const std::size_t text_size = index.text_size();

  if (pattern.empty() || max_edits >= pattern.size()) {
    result.positions.resize(index.row_count());
    for (std::size_t position = 0U; position < index.row_count(); ++position) {
      result.positions[position] = position;
    }
    return result;
  }

  const detail::SeedCandidateSet candidates =
      detail::build_seed_candidates(index, pattern, max_edits);
  result.seed_count = candidates.seed_count;
  result.seed_occurrences = candidates.seed_occurrences;
  result.unique_candidates = candidates.unique_candidates;
  if (result.unique_candidates == 0U) {
    return result;
  }

  const BwtPeriodicSampleTextExtractor extractor{index};
  const std::size_t min_length = pattern.size() - max_edits;
  for (std::size_t start = 0U; start <= text_size; ++start) {
    if (candidates.marked[start] == 0U ||
        text_size - start < min_length) {
      continue;
    }

    const std::size_t end = detail::verification_window_end(
        text_size, start, pattern.size(), max_edits);
    detail::checked_add_counter(
        result.verified_candidates, 1U,
        "candidate-local verified candidate count overflow");
    const SampledBwtTextExtractionResult extracted =
        extractor.extract(start, end);
    detail::checked_add_counter(
        result.extraction_windows, 1U,
        "candidate-local extraction window count overflow");
    detail::checked_add_counter(result.extracted_bytes, extracted.bytes.size(),
                                "candidate-local extracted byte count overflow");
    detail::checked_add_counter(
        result.extraction_lf_steps, extracted.lf_steps,
        "candidate-local extraction LF-step count overflow");

    if (detail::verify_bounded_window(pattern, extracted.bytes, max_edits,
                                      result.verification_dp_cells)) {
      result.positions.push_back(start);
    }
  }
  return result;
}

}  // namespace algorithms::strings
