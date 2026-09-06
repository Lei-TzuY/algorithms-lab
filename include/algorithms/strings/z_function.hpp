#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::strings {

// z[i] is the length of the longest common prefix of input and input.substr(i).
// For non-empty input, z[0] is defined as input.size().
[[nodiscard]] inline std::vector<std::size_t> z_function(
    std::string_view input) {
  const std::size_t size = input.size();
  std::vector<std::size_t> z(size, 0U);
  if (size == 0U) {
    return z;
  }

  z[0] = size;
  std::size_t left = 0U;
  std::size_t right = 0U;

  for (std::size_t index = 1U; index < size; ++index) {
    if (index < right) {
      const std::size_t mirrored = index - left;
      z[index] = std::min(right - index, z[mirrored]);
    }

    while (z[index] < size - index &&
           input[z[index]] == input[index + z[index]]) {
      ++z[index];
    }

    const std::size_t candidate_right = index + z[index];
    if (candidate_right > right) {
      left = index;
      right = candidate_right;
    }
  }

  return z;
}

}  // namespace algorithms::strings
