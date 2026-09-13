#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::dynamic_programming {

struct LcsMatch {
  std::size_t first_index;
  std::size_t second_index;

  friend bool operator==(const LcsMatch&, const LcsMatch&) = default;
};

namespace hirschberg_detail {

[[nodiscard]] inline std::vector<std::size_t> prefix_lengths(
    std::string_view first, std::string_view second) {
  std::vector<std::size_t> previous(second.size() + 1U, 0U);
  std::vector<std::size_t> current(second.size() + 1U, 0U);
  for (const char first_byte : first) {
    current[0] = 0U;
    for (std::size_t column = 0U; column < second.size(); ++column) {
      if (first_byte == second[column]) {
        current[column + 1U] = previous[column] + 1U;
      } else {
        current[column + 1U] =
            std::max(previous[column + 1U], current[column]);
      }
    }
    std::swap(previous, current);
  }
  return previous;
}

[[nodiscard]] inline std::vector<std::size_t> suffix_lengths(
    std::string_view first, std::string_view second) {
  std::vector<std::size_t> next(second.size() + 1U, 0U);
  std::vector<std::size_t> current(second.size() + 1U, 0U);
  for (std::size_t first_pos = first.size(); first_pos > 0U; --first_pos) {
    current[second.size()] = 0U;
    for (std::size_t second_pos = second.size(); second_pos > 0U;
         --second_pos) {
      const std::size_t column = second_pos - 1U;
      if (first[first_pos - 1U] == second[column]) {
        current[column] = next[column + 1U] + 1U;
      } else {
        current[column] = std::max(next[column], current[column + 1U]);
      }
    }
    std::swap(next, current);
  }
  return next;
}

inline void recurse(std::string_view first, std::size_t first_offset,
                    std::string_view second, std::size_t second_offset,
                    std::vector<LcsMatch>& matches) {
  if (first.empty() || second.empty()) return;
  if (first.size() == 1U) {
    for (std::size_t index = 0U; index < second.size(); ++index) {
      if (first[0] == second[index]) {
        matches.push_back(LcsMatch{first_offset, second_offset + index});
        return;
      }
    }
    return;
  }

  const std::size_t first_split = first.size() / 2U;
  std::size_t second_split = 0U;
  {
    const auto prefix = prefix_lengths(first.substr(0U, first_split), second);
    const auto suffix = suffix_lengths(first.substr(first_split), second);
    std::size_t best = 0U;
    for (std::size_t split = 0U; split <= second.size(); ++split) {
      const std::size_t candidate = prefix[split] + suffix[split];
      if (candidate > best) {
        best = candidate;
        second_split = split;
      }
    }
  }

  recurse(first.substr(0U, first_split), first_offset,
          second.substr(0U, second_split), second_offset, matches);
  recurse(first.substr(first_split), first_offset + first_split,
          second.substr(second_split), second_offset + second_split, matches);
}

}  // namespace hirschberg_detail

[[nodiscard]] inline std::vector<LcsMatch> hirschberg_lcs(
    std::string_view first, std::string_view second) {
  std::vector<LcsMatch> matches;
  matches.reserve(std::min(first.size(), second.size()));
  if (second.size() <= first.size()) {
    hirschberg_detail::recurse(first, 0U, second, 0U, matches);
    return matches;
  }

  std::vector<LcsMatch> swapped;
  swapped.reserve(first.size());
  hirschberg_detail::recurse(second, 0U, first, 0U, swapped);
  matches.reserve(swapped.size());
  for (const LcsMatch match : swapped) {
    matches.push_back(LcsMatch{match.second_index, match.first_index});
  }
  return matches;
}

}  // namespace algorithms::dynamic_programming
