#include "algorithms/strings/bwt_seed_verify.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::strings {
namespace {

void checked_add(std::size_t& value, std::size_t delta, const char* message) {
  if (delta > std::numeric_limits<std::size_t>::max() - value) {
    throw std::length_error(message);
  }
  value += delta;
}

bool verify_start(std::string_view pattern, std::string_view text,
                  std::size_t start, std::size_t max_edits,
                  std::size_t& dp_cells) {
  const std::size_t min_length = pattern.size() - max_edits;
  const std::size_t remaining = text.size() - start;
  if (remaining < min_length) {
    return false;
  }

  std::size_t max_length = remaining;
  if (max_edits <= std::numeric_limits<std::size_t>::max() - pattern.size()) {
    max_length = std::min(remaining, pattern.size() + max_edits);
  }

  std::vector<std::size_t> previous(max_length + 1U);
  std::vector<std::size_t> current(max_length + 1U);
  for (std::size_t column = 0U; column <= max_length; ++column) {
    previous[column] = column;
  }

  for (std::size_t row = 1U; row <= pattern.size(); ++row) {
    current[0] = row;
    for (std::size_t column = 1U; column <= max_length; ++column) {
      const std::size_t substitution =
          previous[column - 1U] +
          (pattern[row - 1U] == text[start + column - 1U] ? 0U : 1U);
      const std::size_t deletion = previous[column] + 1U;
      const std::size_t insertion = current[column - 1U] + 1U;
      current[column] = std::min({substitution, deletion, insertion});
      checked_add(dp_cells, 1U,
                  "seed-and-verify edit-distance DP cell count overflow");
    }
    previous.swap(current);
  }

  for (std::size_t length = min_length; length <= max_length; ++length) {
    if (previous[length] <= max_edits) {
      return true;
    }
  }
  return false;
}

}  // namespace

SeedVerifyEditDistanceResult locate_bwt_seed_verify_edit_distance(
    const BwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits) {
  SeedVerifyEditDistanceResult result;
  const std::size_t text_size = index.text_size();

  if (pattern.empty() || max_edits >= pattern.size()) {
    result.positions.resize(index.row_count());
    for (std::size_t position = 0U; position < index.row_count(); ++position) {
      result.positions[position] = position;
    }
    return result;
  }

  const std::size_t part_count = max_edits + 1U;
  result.seed_count = part_count;
  std::vector<std::uint8_t> candidate(index.row_count(), 0U);

  const std::size_t base_length = pattern.size() / part_count;
  const std::size_t longer_parts = pattern.size() % part_count;
  std::size_t seed_begin = 0U;

  for (std::size_t part = 0U; part < part_count; ++part) {
    const std::size_t seed_length = base_length + (part < longer_parts ? 1U : 0U);
    if (seed_length == 0U || seed_length > pattern.size() - seed_begin) {
      throw std::logic_error("seed-and-verify produced invalid seed partition");
    }
    const std::string_view seed = pattern.substr(seed_begin, seed_length);
    const std::vector<std::size_t> occurrences = index.locate(seed);
    checked_add(result.seed_occurrences, occurrences.size(),
                "seed-and-verify seed occurrence count overflow");

    for (const std::size_t occurrence : occurrences) {
      std::size_t lower = 0U;
      if (occurrence >= seed_begin) {
        const std::size_t center = occurrence - seed_begin;
        if (center > max_edits) {
          lower = center - max_edits;
        }
      }

      std::size_t upper = 0U;
      if (occurrence >= seed_begin) {
        const std::size_t center = occurrence - seed_begin;
        if (center > text_size) {
          continue;
        }
        upper = center + std::min(max_edits, text_size - center);
      } else {
        const std::size_t deficit = seed_begin - occurrence;
        if (deficit > max_edits) {
          continue;
        }
        upper = std::min(text_size, max_edits - deficit);
      }

      if (lower > text_size) {
        continue;
      }
      upper = std::min(upper, text_size);
      for (std::size_t start = lower; start <= upper; ++start) {
        candidate[start] = 1U;
      }
    }
    seed_begin += seed_length;
  }

  if (seed_begin != pattern.size()) {
    throw std::logic_error("seed-and-verify seed partition did not cover pattern");
  }

  for (const std::uint8_t marked : candidate) {
    if (marked != 0U) {
      checked_add(result.unique_candidates, 1U,
                  "seed-and-verify candidate count overflow");
    }
  }
  if (result.unique_candidates == 0U) {
    return result;
  }

  const BwtTextReconstructionResult reconstruction = index.reconstruct_text();
  if (reconstruction.text.size() != text_size) {
    throw std::logic_error("seed-and-verify reconstruction size mismatch");
  }
  result.reconstruction_lf_steps = reconstruction.lf_steps;

  const std::string_view recovered{reconstruction.text};
  const std::size_t min_length = pattern.size() - max_edits;
  for (std::size_t start = 0U; start <= text_size; ++start) {
    if (candidate[start] == 0U || text_size - start < min_length) {
      continue;
    }
    checked_add(result.verified_candidates, 1U,
                "seed-and-verify verified candidate count overflow");
    if (verify_start(pattern, recovered, start, max_edits,
                     result.verification_dp_cells)) {
      result.positions.push_back(start);
    }
  }
  return result;
}

}  // namespace algorithms::strings
