#include "algorithms/strings/bwt_index.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace algorithms::strings {

std::vector<std::size_t> BwtByteIndex::locate_run_sampled(
    std::string_view pattern) const {
  const SearchRange range = backward_search(pattern);
  std::vector<std::size_t> positions;
  positions.reserve(range.end - range.begin);

  const std::size_t modulus = row_count();
  for (std::size_t row = range.begin; row < range.end; ++row) {
    std::size_t current = row;
    std::size_t steps = 0U;
    std::size_t sample_position = 0U;
    bool found_sample = false;

    while (!found_sample) {
      if (current == sentinel_row_) {
        // The conceptual sentinel row is the implicit SA=0 sample.
        sample_position = 0U;
        found_sample = true;
        continue;
      }

      const auto sample =
          std::lower_bound(run_toehold_sample_rows_.begin(),
                           run_toehold_sample_rows_.end(), current);
      if (sample != run_toehold_sample_rows_.end() && *sample == current) {
        const std::size_t ordinal = static_cast<std::size_t>(
            sample - run_toehold_sample_rows_.begin());
        if (ordinal >= run_toehold_sample_positions_.size()) {
          throw std::logic_error(
              "BWT run-sampled locate sample ordinal invariant violated");
        }
        sample_position = run_toehold_sample_positions_[ordinal];
        if (sample_position == 0U || sample_position > text_size_) {
          throw std::logic_error(
              "BWT run-sampled locate sample position invariant violated");
        }
        found_sample = true;
        continue;
      }

      current = lf(current);
      ++steps;
      if (steps >= modulus) {
        throw std::logic_error(
            "BWT run-sampled locate failed to reach a sample");
      }
    }

    if (sample_position >= modulus) {
      throw std::logic_error(
          "BWT run-sampled locate reconstruction sample invalid");
    }

    // Each LF step decrements SA modulo n+1. Walking `steps` times from the
    // original row to a known sample therefore means
    // original_SA = sample_SA + steps (mod n+1). Avoid overflowing size_t
    // while computing that modular sum.
    const std::size_t distance_to_wrap = modulus - sample_position;
    const std::size_t position =
        steps < distance_to_wrap ? sample_position + steps
                                 : steps - distance_to_wrap;
    if (position > text_size_) {
      throw std::logic_error(
          "BWT run-sampled locate reconstructed position invalid");
    }
    positions.push_back(position);
  }

  std::sort(positions.begin(), positions.end());
  return positions;
}

MemoizedRunSampledLocateResult BwtByteIndex::locate_run_sampled_memoized(
    std::string_view pattern) const {
  const SearchRange range = backward_search(pattern);
  const std::size_t modulus = row_count();
  const std::size_t unknown = modulus;

  if (sentinel_row_ >= modulus) {
    throw std::logic_error(
        "BWT memoized run-sampled locate sentinel invariant violated");
  }
  if (run_toehold_sample_rows_.size() !=
      run_toehold_sample_positions_.size()) {
    throw std::logic_error(
        "BWT memoized run-sampled locate sample cardinality invariant violated");
  }

  std::vector<std::size_t> memoized_positions(modulus, unknown);
  memoized_positions[sentinel_row_] = 0U;
  std::size_t memoized_row_count = 1U;

  for (std::size_t index = 0U; index < run_toehold_sample_rows_.size();
       ++index) {
    const std::size_t row = run_toehold_sample_rows_[index];
    const std::size_t position = run_toehold_sample_positions_[index];
    if (row >= modulus || row == sentinel_row_ || position == 0U ||
        position > text_size_) {
      throw std::logic_error(
          "BWT memoized run-sampled locate seed invariant violated");
    }
    if (memoized_positions[row] != unknown &&
        memoized_positions[row] != position) {
      throw std::logic_error(
          "BWT memoized run-sampled locate conflicting seed invariant violated");
    }
    if (memoized_positions[row] == unknown) {
      memoized_positions[row] = position;
      ++memoized_row_count;
    }
  }

  MemoizedRunSampledLocateResult result;
  result.seeded_row_count = memoized_row_count;
  result.positions.reserve(range.end - range.begin);

  std::vector<std::size_t> path;
  path.reserve(modulus);

  for (std::size_t row = range.begin; row < range.end; ++row) {
    if (memoized_positions[row] != unknown) {
      ++result.matched_row_cache_hits;
    }

    std::size_t current = row;
    path.clear();
    while (memoized_positions[current] == unknown) {
      path.push_back(current);
      current = lf(current);
      ++result.lf_steps;
      if (path.size() >= modulus) {
        throw std::logic_error(
            "BWT memoized run-sampled locate failed to reach cached row");
      }
    }

    std::size_t position = memoized_positions[current];
    if (position > text_size_) {
      throw std::logic_error(
          "BWT memoized run-sampled locate cached position invariant violated");
    }

    while (!path.empty()) {
      const std::size_t unresolved_row = path.back();
      path.pop_back();
      position = position == text_size_ ? 0U : position + 1U;
      if (memoized_positions[unresolved_row] != unknown) {
        throw std::logic_error(
            "BWT memoized run-sampled locate duplicate fill invariant violated");
      }
      memoized_positions[unresolved_row] = position;
      ++memoized_row_count;
    }

    if (memoized_positions[row] == unknown ||
        memoized_positions[row] > text_size_) {
      throw std::logic_error(
          "BWT memoized run-sampled locate reconstruction invariant violated");
    }
    result.positions.push_back(memoized_positions[row]);
  }

  result.memoized_row_count = memoized_row_count;
  if (result.memoized_row_count < result.seeded_row_count ||
      result.lf_steps != result.memoized_row_count - result.seeded_row_count) {
    throw std::logic_error(
        "BWT memoized run-sampled locate traversal accounting invariant violated");
  }
  if (result.seeded_row_count > modulus ||
      result.lf_steps > modulus - result.seeded_row_count) {
    throw std::logic_error(
        "BWT memoized run-sampled locate LF-step bound invariant violated");
  }

  std::sort(result.positions.begin(), result.positions.end());
  return result;
}

}  // namespace algorithms::strings
