#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace algorithms::strings {

// pi[i] is the length of the longest proper prefix of pattern[0, i + 1)
// that is also a suffix of that same prefix.
[[nodiscard]] inline std::vector<std::size_t> kmp_prefix_function(
    std::string_view pattern) {
  std::vector<std::size_t> prefix(pattern.size(), 0U);
  for (std::size_t index = 1; index < pattern.size(); ++index) {
    std::size_t matched = prefix[index - 1U];
    while (matched > 0U && pattern[index] != pattern[matched]) {
      matched = prefix[matched - 1U];
    }
    if (pattern[index] == pattern[matched]) {
      ++matched;
    }
    prefix[index] = matched;
  }
  return prefix;
}

// Return every starting offset where pattern occurs in text. Occurrences may
// overlap. The empty pattern matches every boundary [0, text.size()].
[[nodiscard]] inline std::vector<std::size_t> kmp_find_all(
    std::string_view text, std::string_view pattern) {
  std::vector<std::size_t> matches;
  if (pattern.empty()) {
    if (text.size() == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("KMP empty-pattern result is too large");
    }
    matches.reserve(text.size() + 1U);
    for (std::size_t boundary = 0; boundary <= text.size(); ++boundary) {
      matches.push_back(boundary);
    }
    return matches;
  }

  const std::vector<std::size_t> prefix = kmp_prefix_function(pattern);
  std::size_t matched = 0U;
  for (std::size_t index = 0; index < text.size(); ++index) {
    while (matched > 0U && text[index] != pattern[matched]) {
      matched = prefix[matched - 1U];
    }
    if (text[index] == pattern[matched]) {
      ++matched;
    }
    if (matched == pattern.size()) {
      matches.push_back(index + 1U - pattern.size());
      matched = prefix[matched - 1U];
    }
  }
  return matches;
}

}  // namespace algorithms::strings
