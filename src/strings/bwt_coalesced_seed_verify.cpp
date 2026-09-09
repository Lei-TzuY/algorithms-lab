#include "algorithms/strings/bwt_coalesced_seed_verify.hpp"

#include "bwt_seed_verify_common.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace algorithms::strings {
namespace {

struct CandidateWindow {
  std::size_t begin;
  std::size_t end;
};

}  // namespace

CoalescedSeedVerifyEditDistanceResult
locate_bwt_coalesced_seed_verify_edit_distance(
    const BwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits) {
  CoalescedSeedVerifyEditDistanceResult result;
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

  const std::size_t min_length = pattern.size() - max_edits;
  std::vector<CandidateWindow> windows;
  windows.reserve(result.unique_candidates);
  for (std::size_t start = 0U; start <= text_size; ++start) {
    if (candidates.marked[start] == 0U ||
        text_size - start < min_length) {
      continue;
    }
    windows.push_back(CandidateWindow{
        start, detail::verification_window_end(text_size, start,
                                               pattern.size(), max_edits)});
    detail::checked_add_counter(
        result.verified_candidates, 1U,
        "coalesced seed verified candidate count overflow");
    detail::checked_add_counter(
        result.candidate_windows, 1U,
        "coalesced seed candidate window count overflow");
  }
  if (windows.empty()) {
    return result;
  }

  const BwtPeriodicSampleTextExtractor extractor{index};
  std::size_t first = 0U;
  while (first < windows.size()) {
    const std::size_t merged_begin = windows[first].begin;
    std::size_t merged_end = windows[first].end;
    std::size_t after = first + 1U;
    while (after < windows.size() && windows[after].begin <= merged_end) {
      merged_end = std::max(merged_end, windows[after].end);
      ++after;
    }

    const SampledBwtTextExtractionResult extracted =
        extractor.extract(merged_begin, merged_end);
    const std::size_t expected_size = merged_end - merged_begin;
    if (extracted.bytes.size() != expected_size) {
      throw std::logic_error("coalesced seed extraction size mismatch");
    }
    detail::checked_add_counter(
        result.merged_extraction_windows, 1U,
        "coalesced seed merged extraction window count overflow");
    detail::checked_add_counter(result.extracted_bytes, extracted.bytes.size(),
                                "coalesced seed extracted byte count overflow");
    detail::checked_add_counter(
        result.extraction_lf_steps, extracted.lf_steps,
        "coalesced seed extraction LF-step count overflow");

    const std::string_view merged_bytes{extracted.bytes};
    for (std::size_t index_in_group = first; index_in_group < after;
         ++index_in_group) {
      const CandidateWindow window = windows[index_in_group];
      const std::size_t offset = window.begin - merged_begin;
      const std::size_t length = window.end - window.begin;
      if (offset > merged_bytes.size() ||
          length > merged_bytes.size() - offset) {
        throw std::logic_error(
            "coalesced seed candidate window escaped merged extraction");
      }
      if (detail::verify_bounded_window(
              pattern, merged_bytes.substr(offset, length), max_edits,
              result.verification_dp_cells)) {
        result.positions.push_back(window.begin);
      }
    }
    first = after;
  }
  return result;
}

}  // namespace algorithms::strings
