#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct ManacherPalindromeRadii {
  // odd_radius[i] is the number of bytes from center i to either side,
  // including the center itself. It describes
  // [i - odd_radius[i] + 1, i + odd_radius[i]).
  std::vector<std::size_t> odd_radius;

  // even_radius[i] is the number of bytes on each side of the center between
  // i-1 and i. It describes [i - even_radius[i], i + even_radius[i]).
  std::vector<std::size_t> even_radius;

  // Canonical longest palindrome: maximum length, breaking ties toward the
  // smallest start offset. The empty input reports {0, 0}.
  std::size_t longest_start{};
  std::size_t longest_length{};

  friend bool operator==(const ManacherPalindromeRadii&,
                         const ManacherPalindromeRadii&) = default;
};

[[nodiscard]] inline ManacherPalindromeRadii manacher_palindrome_radii(
    std::string_view bytes) {
  const std::size_t n = bytes.size();
  if (n > std::numeric_limits<std::size_t>::max() / 2U) {
    throw std::length_error("Manacher input is too large");
  }

  ManacherPalindromeRadii result;
  result.odd_radius.assign(n, 0U);
  result.even_radius.assign(n, 0U);

  // Rightmost odd palindrome is [left, right). For each new center i that is
  // still inside it, mirror reuse gives a radius that is already known to be
  // valid without comparing bytes again. Only comparisons at/beyond `right`
  // can advance the boundary.
  std::size_t left = 0U;
  std::size_t right = 0U;
  for (std::size_t i = 0U; i < n; ++i) {
    std::size_t radius = 1U;
    if (i < right) {
      const std::size_t mirror = left + (right - 1U - i);
      radius = std::min(result.odd_radius[mirror], right - i);
    }

    while (radius <= i && radius < n - i &&
           bytes[i - radius] == bytes[i + radius]) {
      ++radius;
    }
    result.odd_radius[i] = radius;

    if (i + radius > right) {
      left = i - (radius - 1U);
      right = i + radius;
    }
  }

  // Rightmost even palindrome is [left, right), centered between positions
  // c-1 and c for some already-processed c. If i is inside that palindrome's
  // right half, its reflected center is `left + (right - i)`.
  left = 0U;
  right = 0U;
  for (std::size_t i = 0U; i < n; ++i) {
    std::size_t radius = 0U;
    if (i < right) {
      const std::size_t mirror = left + (right - i);
      radius = std::min(result.even_radius[mirror], right - i);
    }

    while (radius < i && radius < n - i &&
           bytes[i - radius - 1U] == bytes[i + radius]) {
      ++radius;
    }
    result.even_radius[i] = radius;

    if (i + radius > right) {
      left = i - radius;
      right = i + radius;
    }
  }

  auto consider = [&result](std::size_t start, std::size_t length) {
    if (length > result.longest_length ||
        (length == result.longest_length && start < result.longest_start)) {
      result.longest_start = start;
      result.longest_length = length;
    }
  };

  for (std::size_t i = 0U; i < n; ++i) {
    const std::size_t odd = result.odd_radius[i];
    consider(i - (odd - 1U), 2U * odd - 1U);

    const std::size_t even = result.even_radius[i];
    if (even != 0U) {
      consider(i - even, 2U * even);
    }
  }

  return result;
}

}  // namespace algorithms::strings
