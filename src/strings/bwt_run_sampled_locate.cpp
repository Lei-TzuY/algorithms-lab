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

}  // namespace algorithms::strings
