#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::approximation {

struct WeightedCoverSet {
  std::uint64_t cost;
  std::vector<std::size_t> elements;
};

struct WeightedSetCoverStep {
  std::size_t set_index;
  std::uint64_t cost;
  std::size_t newly_covered;
  std::size_t covered_after;

  friend bool operator==(const WeightedSetCoverStep&,
                         const WeightedSetCoverStep&) = default;
};

struct WeightedSetCoverResult {
  std::vector<std::size_t> selected_set_indices;
  std::vector<std::optional<std::size_t>> first_cover_set;
  std::vector<WeightedSetCoverStep> steps;
  std::uint64_t total_cost;

  friend bool operator==(const WeightedSetCoverResult&,
                         const WeightedSetCoverResult&) = default;
};

namespace weighted_set_cover_detail {

static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));

// Compare a/b with c/d exactly without cross multiplication. All denominators
// must be positive. The Euclidean/continued-fraction transform reverses the
// comparison each time reciprocals are taken.
[[nodiscard]] inline int compare_nonnegative_ratios(std::uint64_t a,
                                                     std::uint64_t b,
                                                     std::uint64_t c,
                                                     std::uint64_t d) {
  if (b == 0U || d == 0U) {
    throw std::logic_error("set-cover ratio denominator must be positive");
  }

  bool reversed = false;
  while (true) {
    const std::uint64_t aq = a / b;
    const std::uint64_t cq = c / d;
    if (aq != cq) {
      const int direct = aq < cq ? -1 : 1;
      return reversed ? -direct : direct;
    }

    a %= b;
    c %= d;
    if (a == 0U || c == 0U) {
      if (a == 0U && c == 0U) {
        return 0;
      }
      const int direct = a == 0U ? -1 : 1;
      return reversed ? -direct : direct;
    }

    std::swap(a, b);
    std::swap(c, d);
    reversed = !reversed;
  }
}

}  // namespace weighted_set_cover_detail

// Deterministic weighted greedy set cover over the dense universe [0,n).
// Returns nullopt exactly when the supplied family cannot cover the universe.
// Ties between equal cost/newly-covered ratios use the smaller input set index.
[[nodiscard]] inline std::optional<WeightedSetCoverResult>
greedy_weighted_set_cover(std::size_t universe_size,
                          const std::vector<WeightedCoverSet>& sets) {
  const std::size_t no_set = std::numeric_limits<std::size_t>::max();

  // Normalize duplicate element mentions without sorting: each input set is
  // treated mathematically as a set, while preserving first-seen element order.
  std::vector<std::vector<std::size_t>> normalized(sets.size());
  std::vector<std::size_t> seen(universe_size, no_set);
  for (std::size_t set_index = 0U; set_index < sets.size(); ++set_index) {
    auto& output = normalized[set_index];
    output.reserve(sets[set_index].elements.size());
    for (const std::size_t element : sets[set_index].elements) {
      if (element >= universe_size) {
        throw std::out_of_range("set-cover element outside universe");
      }
      if (seen[element] != set_index) {
        seen[element] = set_index;
        output.push_back(element);
      }
    }
  }

  WeightedSetCoverResult result;
  result.first_cover_set.resize(universe_size);
  result.total_cost = 0U;
  if (universe_size == 0U) {
    return result;
  }

  std::vector<bool> covered(universe_size, false);
  std::size_t covered_count = 0U;

  while (covered_count < universe_size) {
    std::size_t best_index = no_set;
    std::size_t best_gain = 0U;

    for (std::size_t set_index = 0U; set_index < sets.size(); ++set_index) {
      std::size_t gain = 0U;
      for (const std::size_t element : normalized[set_index]) {
        if (!covered[element]) {
          ++gain;
        }
      }
      if (gain == 0U) {
        continue;
      }

      if (best_index == no_set) {
        best_index = set_index;
        best_gain = gain;
        continue;
      }

      const int comparison =
          weighted_set_cover_detail::compare_nonnegative_ratios(
              sets[set_index].cost, static_cast<std::uint64_t>(gain),
              sets[best_index].cost, static_cast<std::uint64_t>(best_gain));
      if (comparison < 0 || (comparison == 0 && set_index < best_index)) {
        best_index = set_index;
        best_gain = gain;
      }
    }

    if (best_index == no_set) {
      return std::nullopt;
    }

    const std::uint64_t selected_cost = sets[best_index].cost;
    if (selected_cost >
        std::numeric_limits<std::uint64_t>::max() - result.total_cost) {
      throw std::overflow_error("set-cover selected cost is not representable");
    }

    std::size_t applied_gain = 0U;
    for (const std::size_t element : normalized[best_index]) {
      if (!covered[element]) {
        covered[element] = true;
        result.first_cover_set[element] = best_index;
        ++applied_gain;
      }
    }
    if (applied_gain != best_gain) {
      throw std::logic_error("set-cover gain changed during selection");
    }

    covered_count += applied_gain;
    result.total_cost += selected_cost;
    result.selected_set_indices.push_back(best_index);
    result.steps.push_back(WeightedSetCoverStep{
        best_index, selected_cost, applied_gain, covered_count});
  }

  return result;
}

}  // namespace algorithms::approximation
