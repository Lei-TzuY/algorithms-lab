#include "algorithms/combinatorial/matroid_intersection.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <stdexcept>
#include <utility>

namespace algorithms::combinatorial {
namespace {

constexpr std::size_t kNoParent = std::numeric_limits<std::size_t>::max();

std::vector<std::size_t> materialize_selected(const std::vector<bool>& in_set) {
  std::vector<std::size_t> selected;
  selected.reserve(in_set.size());
  for (std::size_t element = 0; element < in_set.size(); ++element) {
    if (in_set[element]) {
      selected.push_back(element);
    }
  }
  return selected;
}

std::vector<std::size_t> materialize_add(const std::vector<bool>& in_set,
                                         std::size_t added) {
  std::vector<std::size_t> subset;
  subset.reserve(in_set.size());
  for (std::size_t element = 0; element < in_set.size(); ++element) {
    if (in_set[element] || element == added) {
      subset.push_back(element);
    }
  }
  return subset;
}

std::vector<std::size_t> materialize_exchange(const std::vector<bool>& in_set,
                                              std::size_t removed,
                                              std::size_t added) {
  std::vector<std::size_t> subset;
  subset.reserve(in_set.size());
  for (std::size_t element = 0; element < in_set.size(); ++element) {
    if ((in_set[element] && element != removed) || element == added) {
      subset.push_back(element);
    }
  }
  return subset;
}

bool call_oracle(const MatroidIndependenceOracle& oracle,
                 const std::vector<std::size_t>& subset,
                 std::size_t& call_count) {
  if (call_count == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error("matroid oracle call counter overflow");
  }
  ++call_count;
  return oracle(subset);
}

}  // namespace

MatroidIntersectionResult maximum_cardinality_matroid_intersection(
    std::size_t ground_size, const MatroidIndependenceOracle& first,
    const MatroidIndependenceOracle& second) {
  if (!first || !second) {
    throw std::invalid_argument("matroid independence oracle is empty");
  }

  MatroidIntersectionResult result;
  const std::vector<std::size_t> empty;
  if (!call_oracle(first, empty, result.first_oracle_calls) ||
      !call_oracle(second, empty, result.second_oracle_calls)) {
    throw std::invalid_argument(
        "matroid independence oracle must accept the empty set");
  }

  std::vector<bool> in_set(ground_size, false);

  while (true) {
    std::vector<bool> is_sink(ground_size, false);
    std::vector<std::size_t> parent(ground_size, kNoParent);
    std::vector<bool> visited(ground_size, false);
    std::deque<std::size_t> queue;

    for (std::size_t element = 0; element < ground_size; ++element) {
      if (in_set[element]) {
        continue;
      }
      const auto added = materialize_add(in_set, element);
      if (call_oracle(first, added, result.first_oracle_calls)) {
        visited[element] = true;
        queue.push_back(element);
      }
      if (call_oracle(second, added, result.second_oracle_calls)) {
        is_sink[element] = true;
      }
    }

    std::size_t sink = kNoParent;
    while (!queue.empty() && sink == kNoParent) {
      const std::size_t current = queue.front();
      queue.pop_front();

      if (!in_set[current] && is_sink[current]) {
        sink = current;
        break;
      }

      if (!in_set[current]) {
        // Second-matroid exchange arcs: outside -> inside.
        for (std::size_t inside = 0; inside < ground_size; ++inside) {
          if (!in_set[inside] || visited[inside]) {
            continue;
          }
          const auto exchanged = materialize_exchange(in_set, inside, current);
          if (call_oracle(second, exchanged, result.second_oracle_calls)) {
            visited[inside] = true;
            parent[inside] = current;
            queue.push_back(inside);
          }
        }
      } else {
        // First-matroid exchange arcs: inside -> outside.
        for (std::size_t outside = 0; outside < ground_size; ++outside) {
          if (in_set[outside] || visited[outside]) {
            continue;
          }
          const auto exchanged = materialize_exchange(in_set, current, outside);
          if (call_oracle(first, exchanged, result.first_oracle_calls)) {
            visited[outside] = true;
            parent[outside] = current;
            queue.push_back(outside);
          }
        }
      }
    }

    if (sink == kNoParent) {
      break;
    }

    const std::size_t previous_size =
        static_cast<std::size_t>(std::count(in_set.begin(), in_set.end(), true));
    for (std::size_t vertex = sink;; vertex = parent[vertex]) {
      in_set[vertex] = !in_set[vertex];
      if (parent[vertex] == kNoParent) {
        break;
      }
    }

    const std::size_t new_size =
        static_cast<std::size_t>(std::count(in_set.begin(), in_set.end(), true));
    if (new_size != previous_size + 1U) {
      throw std::logic_error("matroid augmenting path did not increase cardinality");
    }
    if (result.augmentation_count == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("matroid augmentation counter overflow");
    }
    ++result.augmentation_count;

    const auto selected = materialize_selected(in_set);
    if (!call_oracle(first, selected, result.first_oracle_calls) ||
        !call_oracle(second, selected, result.second_oracle_calls)) {
      throw std::logic_error(
          "matroid independence oracle contract violated after augmentation");
    }
  }

  result.selected_elements = materialize_selected(in_set);
  return result;
}

}  // namespace algorithms::combinatorial
