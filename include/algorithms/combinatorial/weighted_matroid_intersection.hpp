#pragma once

#include "algorithms/combinatorial/matroid_intersection.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

struct WeightedMatroidIntersectionResult {
  std::vector<std::size_t> selected_elements;
  std::int64_t total_weight{};
  std::size_t augmentation_count{};
  std::size_t first_oracle_calls{};
  std::size_t second_oracle_calls{};

  friend bool operator==(const WeightedMatroidIntersectionResult&,
                         const WeightedMatroidIntersectionResult&) = default;
};

namespace weighted_matroid_detail {

constexpr std::size_t kNoParent = std::numeric_limits<std::size_t>::max();

inline std::int64_t checked_add(std::int64_t left, std::int64_t right) {
  if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
      (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
    throw std::overflow_error("weighted matroid integer overflow");
  }
  return left + right;
}

inline std::int64_t checked_negate(std::int64_t value) {
  if (value == std::numeric_limits<std::int64_t>::min()) {
    throw std::overflow_error("weighted matroid weight magnitude overflow");
  }
  return -value;
}

inline void validate_weight_domain(const std::vector<std::int64_t>& weights) {
  std::uint64_t absolute_sum = 0;
  const auto limit = static_cast<std::uint64_t>(
      std::numeric_limits<std::int64_t>::max());
  for (const std::int64_t value : weights) {
    if (value == std::numeric_limits<std::int64_t>::min()) {
      throw std::overflow_error("weighted matroid weight magnitude overflow");
    }
    const auto magnitude = static_cast<std::uint64_t>(value < 0 ? -value : value);
    if (magnitude > limit - absolute_sum) {
      throw std::overflow_error("weighted matroid total weight domain overflow");
    }
    absolute_sum += magnitude;
  }
}

inline std::vector<std::size_t> materialize_selected(
    const std::vector<bool>& in_set) {
  std::vector<std::size_t> selected;
  selected.reserve(in_set.size());
  for (std::size_t element = 0; element < in_set.size(); ++element) {
    if (in_set[element]) {
      selected.push_back(element);
    }
  }
  return selected;
}

inline std::vector<std::size_t> materialize_add(
    const std::vector<bool>& in_set, std::size_t added) {
  std::vector<std::size_t> subset;
  subset.reserve(in_set.size());
  for (std::size_t element = 0; element < in_set.size(); ++element) {
    if (in_set[element] || element == added) {
      subset.push_back(element);
    }
  }
  return subset;
}

inline std::vector<std::size_t> materialize_exchange(
    const std::vector<bool>& in_set, std::size_t removed, std::size_t added) {
  std::vector<std::size_t> subset;
  subset.reserve(in_set.size());
  for (std::size_t element = 0; element < in_set.size(); ++element) {
    if ((in_set[element] && element != removed) || element == added) {
      subset.push_back(element);
    }
  }
  return subset;
}

inline bool call_oracle(const MatroidIndependenceOracle& oracle,
                        const std::vector<std::size_t>& subset,
                        std::size_t& counter) {
  if (counter == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error("weighted matroid oracle counter overflow");
  }
  ++counter;
  return oracle(subset);
}

struct ExchangeArc {
  std::size_t from{};
  std::size_t to{};
  std::int64_t cost{};
};

struct Distance {
  bool reachable{};
  std::int64_t cost{};
  std::size_t hops{};
  std::size_t parent{kNoParent};
};

inline bool better(std::int64_t cost, std::size_t hops,
                   const Distance& current) {
  return !current.reachable || cost < current.cost ||
         (cost == current.cost && hops < current.hops);
}

inline std::int64_t selected_weight(const std::vector<bool>& in_set,
                                    const std::vector<std::int64_t>& weights) {
  std::int64_t total = 0;
  for (std::size_t element = 0; element < in_set.size(); ++element) {
    if (in_set[element]) {
      total = checked_add(total, weights[element]);
    }
  }
  return total;
}

}  // namespace weighted_matroid_detail

// Lexicographic weighted matroid intersection: first maximize cardinality, then
// maximize total signed weight among all maximum-cardinality common independent
// sets. General matroid axioms remain a caller precondition just as for the
// unweighted baseline. Every oracle input is increasingly sorted.
//
// The implementation maintains a maximum-weight common independent set of the
// current cardinality. Its exchange graph uses the same orientation as the
// unweighted solver. An outside element has node cost -weight, an inside
// element has node cost +weight, so an augmenting path's cost is the negative
// of its weight gain. Bellman-Ford selects minimum cost, then minimum hops.
// The weighted augmenting-path theorem makes the toggled set weight-optimal at
// the next cardinality. No augmenting path therefore certifies maximum
// cardinality.
//
// Bounded-exact arithmetic contract: sum(abs(weights)) must fit int64_t. This
// guarantees every simple exchange-path cost and every subset total is exactly
// representable. Inputs outside that domain throw std::overflow_error.
[[nodiscard]] inline WeightedMatroidIntersectionResult
maximum_weight_matroid_intersection(
    const std::vector<std::int64_t>& weights,
    const MatroidIndependenceOracle& first,
    const MatroidIndependenceOracle& second) {
  using namespace weighted_matroid_detail;
  if (!first || !second) {
    throw std::invalid_argument("matroid independence oracle is empty");
  }
  validate_weight_domain(weights);

  WeightedMatroidIntersectionResult result;
  const std::vector<std::size_t> empty;
  if (!call_oracle(first, empty, result.first_oracle_calls) ||
      !call_oracle(second, empty, result.second_oracle_calls)) {
    throw std::invalid_argument(
        "matroid independence oracle must accept the empty set");
  }

  const std::size_t n = weights.size();
  std::vector<bool> in_set(n, false);

  while (true) {
    std::vector<bool> is_source(n, false);
    std::vector<bool> is_sink(n, false);
    std::vector<ExchangeArc> arcs;
    if (n != 0U && n <= std::numeric_limits<std::size_t>::max() / n) {
      arcs.reserve(n * n);
    }

    for (std::size_t element = 0; element < n; ++element) {
      if (in_set[element]) {
        continue;
      }
      const auto added = materialize_add(in_set, element);
      is_source[element] =
          call_oracle(first, added, result.first_oracle_calls);
      is_sink[element] = call_oracle(second, added, result.second_oracle_calls);
    }

    for (std::size_t outside = 0; outside < n; ++outside) {
      if (in_set[outside]) {
        continue;
      }
      for (std::size_t inside = 0; inside < n; ++inside) {
        if (!in_set[inside]) {
          continue;
        }
        const auto exchanged = materialize_exchange(in_set, inside, outside);
        if (call_oracle(second, exchanged, result.second_oracle_calls)) {
          arcs.push_back(ExchangeArc{outside, inside, weights[inside]});
        }
        if (call_oracle(first, exchanged, result.first_oracle_calls)) {
          arcs.push_back(
              ExchangeArc{inside, outside, checked_negate(weights[outside])});
        }
      }
    }

    std::vector<Distance> distance(n);
    for (std::size_t source = 0; source < n; ++source) {
      if (is_source[source]) {
        distance[source] = Distance{true, checked_negate(weights[source]), 1U,
                                    kNoParent};
      }
    }

    for (std::size_t iteration = 0; iteration + 1U < n; ++iteration) {
      bool changed = false;
      for (const ExchangeArc& arc : arcs) {
        if (!distance[arc.from].reachable) {
          continue;
        }
        if (distance[arc.from].hops ==
            std::numeric_limits<std::size_t>::max()) {
          throw std::overflow_error("weighted matroid path length overflow");
        }
        const std::int64_t candidate_cost =
            checked_add(distance[arc.from].cost, arc.cost);
        const std::size_t candidate_hops = distance[arc.from].hops + 1U;
        if (better(candidate_cost, candidate_hops, distance[arc.to])) {
          distance[arc.to] = Distance{true, candidate_cost, candidate_hops,
                                      arc.from};
          changed = true;
        } else if (distance[arc.to].reachable &&
                   candidate_cost == distance[arc.to].cost &&
                   candidate_hops == distance[arc.to].hops &&
                   arc.from < distance[arc.to].parent) {
          distance[arc.to].parent = arc.from;
        }
      }
      if (!changed) {
        break;
      }
    }

    // A reachable lexicographically improving cycle contradicts the invariant
    // that the current cardinality already has maximum possible total weight.
    for (const ExchangeArc& arc : arcs) {
      if (!distance[arc.from].reachable) {
        continue;
      }
      const std::int64_t candidate_cost =
          checked_add(distance[arc.from].cost, arc.cost);
      const std::size_t candidate_hops = distance[arc.from].hops + 1U;
      if (better(candidate_cost, candidate_hops, distance[arc.to])) {
        throw std::logic_error(
            "weighted matroid exchange graph contains an improving cycle");
      }
    }

    std::size_t sink = kNoParent;
    for (std::size_t element = 0; element < n; ++element) {
      if (in_set[element] || !is_sink[element] ||
          !distance[element].reachable) {
        continue;
      }
      if (sink == kNoParent || distance[element].cost < distance[sink].cost ||
          (distance[element].cost == distance[sink].cost &&
           (distance[element].hops < distance[sink].hops ||
            (distance[element].hops == distance[sink].hops && element < sink)))) {
        sink = element;
      }
    }
    if (sink == kNoParent) {
      break;
    }

    const std::size_t previous_size = static_cast<std::size_t>(
        std::count(in_set.begin(), in_set.end(), true));
    for (std::size_t vertex = sink;; vertex = distance[vertex].parent) {
      in_set[vertex] = !in_set[vertex];
      if (distance[vertex].parent == kNoParent) {
        break;
      }
    }

    const auto selected = materialize_selected(in_set);
    if (selected.size() != previous_size + 1U) {
      throw std::logic_error(
          "weighted matroid augmenting path did not increase cardinality");
    }
    if (!call_oracle(first, selected, result.first_oracle_calls) ||
        !call_oracle(second, selected, result.second_oracle_calls)) {
      throw std::logic_error(
          "matroid independence oracle contract violated after augmentation");
    }
    if (result.augmentation_count == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("weighted matroid augmentation counter overflow");
    }
    ++result.augmentation_count;
  }

  result.selected_elements = materialize_selected(in_set);
  result.total_weight = selected_weight(in_set, weights);
  return result;
}

}  // namespace algorithms::combinatorial
