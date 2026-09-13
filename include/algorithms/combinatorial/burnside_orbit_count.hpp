#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

struct BurnsideElementDiagnostic {
  std::size_t cycle_count{};
  std::uint64_t fixed_colorings{};

  friend bool operator==(const BurnsideElementDiagnostic&,
                         const BurnsideElementDiagnostic&) = default;
};

struct BurnsideOrbitCountResult {
  std::uint64_t orbit_count{};
  std::uint64_t fixed_coloring_sum{};
  std::vector<BurnsideElementDiagnostic> elements;
};

namespace detail {

inline std::uint64_t checked_power(std::uint64_t base, std::size_t exponent) {
  std::uint64_t result = 1;
  for (std::size_t i = 0; i < exponent; ++i) {
    if (base != 0 && result > std::numeric_limits<std::uint64_t>::max() / base) {
      throw std::overflow_error("Burnside fixed-coloring count exceeds uint64_t");
    }
    result *= base;
  }
  return result;
}

inline std::vector<std::size_t> compose_permutations(
    const std::vector<std::size_t>& first,
    const std::vector<std::size_t>& second) {
  std::vector<std::size_t> composed(first.size());
  for (std::size_t i = 0; i < first.size(); ++i) {
    composed[i] = first[second[i]];
  }
  return composed;
}

inline std::size_t count_cycles(const std::vector<std::size_t>& permutation) {
  std::vector<bool> visited(permutation.size(), false);
  std::size_t cycles = 0;
  for (std::size_t start = 0; start < permutation.size(); ++start) {
    if (visited[start]) {
      continue;
    }
    ++cycles;
    std::size_t current = start;
    while (!visited[current]) {
      visited[current] = true;
      current = permutation[current];
    }
  }
  return cycles;
}

inline void validate_group(
    std::size_t position_count,
    const std::vector<std::vector<std::size_t>>& group_elements) {
  if (group_elements.empty()) {
    throw std::invalid_argument("Burnside group must contain at least identity");
  }

  std::set<std::vector<std::size_t>> elements;
  for (const auto& permutation : group_elements) {
    if (permutation.size() != position_count) {
      throw std::invalid_argument("Burnside permutation size mismatch");
    }
    std::vector<bool> seen(position_count, false);
    for (const std::size_t image : permutation) {
      if (image >= position_count || seen[image]) {
        throw std::invalid_argument("Burnside element is not a permutation");
      }
      seen[image] = true;
    }
    if (!elements.insert(permutation).second) {
      throw std::invalid_argument("Burnside group elements must be unique");
    }
  }

  std::vector<std::size_t> identity(position_count);
  for (std::size_t i = 0; i < position_count; ++i) {
    identity[i] = i;
  }
  if (!elements.contains(identity)) {
    throw std::invalid_argument("Burnside group must contain identity");
  }

  for (const auto& first : group_elements) {
    for (const auto& second : group_elements) {
      if (!elements.contains(compose_permutations(first, second))) {
        throw std::invalid_argument("Burnside elements are not closed under composition");
      }
    }
  }
}

}  // namespace detail

inline BurnsideOrbitCountResult count_color_orbits_burnside(
    std::size_t position_count, std::uint64_t color_count,
    const std::vector<std::vector<std::size_t>>& group_elements) {
  if (color_count == 0) {
    throw std::invalid_argument("Burnside color count must be positive");
  }
  detail::validate_group(position_count, group_elements);

  BurnsideOrbitCountResult result;
  result.elements.reserve(group_elements.size());

  for (const auto& permutation : group_elements) {
    const std::size_t cycles = detail::count_cycles(permutation);
    const std::uint64_t fixed = detail::checked_power(color_count, cycles);
    if (result.fixed_coloring_sum >
        std::numeric_limits<std::uint64_t>::max() - fixed) {
      throw std::overflow_error("Burnside fixed-coloring sum exceeds uint64_t");
    }
    result.fixed_coloring_sum += fixed;
    result.elements.push_back(BurnsideElementDiagnostic{cycles, fixed});
  }

  const auto group_order = static_cast<std::uint64_t>(group_elements.size());
  if (static_cast<std::size_t>(group_order) != group_elements.size()) {
    throw std::overflow_error("Burnside group order exceeds uint64_t");
  }
  if (result.fixed_coloring_sum % group_order != 0) {
    throw std::logic_error("Burnside fixed-point sum is not divisible by group order");
  }
  result.orbit_count = result.fixed_coloring_sum / group_order;
  return result;
}

}  // namespace algorithms::combinatorial
