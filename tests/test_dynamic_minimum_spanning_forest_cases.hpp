#pragma once

#include "algorithms/graphs/dynamic_minimum_spanning_forest.hpp"
#include "test_framework.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace dynamic_msf_test_detail {

struct OracleEdge {
  std::size_t first;
  std::size_t second;
  std::int64_t weight;
  bool active;
};

class Dsu {
 public:
  explicit Dsu(std::size_t count) : parent_(count), size_(count, 1U) {
    for (std::size_t index = 0U; index < count; ++index) {
      parent_[index] = index;
    }
  }

  std::size_t find(std::size_t value) {
    while (parent_[value] != value) {
      value = parent_[value];
    }
    return value;
  }

  bool unite(std::size_t first, std::size_t second) {
    first = find(first);
    second = find(second);
    if (first == second) {
      return false;
    }
    if (size_[first] < size_[second]) {
      std::swap(first, second);
    }
    parent_[second] = first;
    size_[first] += size_[second];
    return true;
  }

 private:
  std::vector<std::size_t> parent_;
  std::vector<std::size_t> size_;
};

struct OracleResult {
  std::int64_t total_weight;
  std::size_t component_count;
};

inline OracleResult exhaustive_optimum(
    std::size_t vertex_count, const std::vector<OracleEdge>& edges) {
  Dsu active_components(vertex_count);
  std::vector<std::size_t> candidates;
  for (std::size_t index = 0U; index < edges.size(); ++index) {
    const OracleEdge& edge = edges[index];
    if (edge.active && edge.first != edge.second) {
      static_cast<void>(active_components.unite(edge.first, edge.second));
      candidates.push_back(index);
    }
  }

  std::size_t component_count = 0U;
  for (std::size_t vertex = 0U; vertex < vertex_count; ++vertex) {
    if (active_components.find(vertex) == vertex) {
      ++component_count;
    }
  }
  const std::size_t required_edges = vertex_count - component_count;
  if (candidates.size() > 20U) {
    throw std::logic_error("dynamic MSF exhaustive oracle domain exceeded");
  }

  bool found = required_edges == 0U;
  std::int64_t best_weight = 0;
  const std::uint64_t subset_count = std::uint64_t{1} << candidates.size();
  for (std::uint64_t mask = 0U; mask < subset_count; ++mask) {
    if (static_cast<std::size_t>(std::popcount(mask)) != required_edges) {
      continue;
    }
    Dsu forest(vertex_count);
    std::int64_t weight = 0;
    bool valid = true;
    for (std::size_t offset = 0U; offset < candidates.size(); ++offset) {
      if ((mask & (std::uint64_t{1} << offset)) == 0U) {
        continue;
      }
      const OracleEdge& edge = edges[candidates[offset]];
      if (!forest.unite(edge.first, edge.second)) {
        valid = false;
        break;
      }
      weight += edge.weight;  // Test domain keeps every oracle sum small.
    }
    for (std::size_t first = 0U; first < vertex_count && valid; ++first) {
      for (std::size_t second = 0U; second < vertex_count; ++second) {
        if ((active_components.find(first) == active_components.find(second)) !=
            (forest.find(first) == forest.find(second))) {
          valid = false;
          break;
        }
      }
    }
    if (valid && (!found || weight < best_weight)) {
      found = true;
      best_weight = weight;
    }
  }
  if (!found) {
    throw std::logic_error("dynamic MSF exhaustive oracle found no forest");
  }
  return OracleResult{best_weight, component_count};
}

}  // namespace dynamic_msf_test_detail

TEST_CASE(dynamic_msf_cycle_and_cut_replacement_are_deterministic) {
  algorithms::graphs::DynamicMinimumSpanningForest forest(5U);
  const auto e0 = forest.add_edge(0U, 1U, 5);
  const auto e1 = forest.add_edge(1U, 2U, 5);
  const auto e2 = forest.add_edge(0U, 2U, 2);
  const auto loop = forest.add_edge(2U, 2U, -100);
  const auto parallel = forest.add_edge(0U, 1U, 4);

  REQUIRE(!forest.in_forest(e0));
  REQUIRE(!forest.in_forest(e1));
  REQUIRE(forest.in_forest(e2));
  REQUIRE(!forest.in_forest(loop));
  REQUIRE(forest.in_forest(parallel));
  REQUIRE(!forest.in_forest(e0));
  REQUIRE_EQ(forest.total_weight(), 6);
  REQUIRE_EQ(forest.component_count(), 3U);
  REQUIRE(forest.valid_structure());

  forest.remove_edge(e2);
  REQUIRE(forest.in_forest(e1));
  REQUIRE_EQ(forest.total_weight(), 9);
  REQUIRE(forest.connected(0U, 2U));
  REQUIRE(forest.valid_structure());

  forest.remove_edge(e1);
  REQUIRE(!forest.connected(0U, 2U));
  REQUIRE_EQ(forest.component_count(), 4U);
  REQUIRE(forest.valid_structure());

  REQUIRE_THROWS_AS(forest.remove_edge(e1), std::invalid_argument);
  REQUIRE_THROWS_AS(forest.edge_state(99U), std::out_of_range);
  REQUIRE_THROWS_AS(forest.connected(0U, 99U), std::out_of_range);
}

TEST_CASE(dynamic_msf_exact_total_weight_handles_cancellation_and_overflow) {
  algorithms::graphs::DynamicMinimumSpanningForest cancellation(4U);
  const auto max_edge = cancellation.add_edge(
      0U, 1U, std::numeric_limits<std::int64_t>::max());
  static_cast<void>(max_edge);
  static_cast<void>(cancellation.add_edge(1U, 2U, 1));
  static_cast<void>(cancellation.add_edge(2U, 3U, -1));
  REQUIRE_EQ(cancellation.total_weight(),
             std::numeric_limits<std::int64_t>::max());
  REQUIRE(cancellation.valid_structure());

  algorithms::graphs::DynamicMinimumSpanningForest minimum(2U);
  static_cast<void>(minimum.add_edge(
      0U, 1U, std::numeric_limits<std::int64_t>::min()));
  REQUIRE_EQ(minimum.total_weight(),
             std::numeric_limits<std::int64_t>::min());
  REQUIRE(minimum.valid_structure());

  algorithms::graphs::DynamicMinimumSpanningForest overflow(4U);
  const auto first = overflow.add_edge(
      0U, 1U, std::numeric_limits<std::int64_t>::max());
  static_cast<void>(overflow.add_edge(
      2U, 3U, std::numeric_limits<std::int64_t>::max()));
  REQUIRE_THROWS_AS(overflow.total_weight(), std::overflow_error);
  overflow.remove_edge(first);
  REQUIRE_EQ(overflow.total_weight(),
             std::numeric_limits<std::int64_t>::max());
  REQUIRE(overflow.valid_structure());
}

TEST_CASE(dynamic_msf_randomized_against_exhaustive_forest_oracle) {
  std::mt19937_64 random(0xD1A4C0DE5EEDULL);
  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(random() % 6U);
    algorithms::graphs::DynamicMinimumSpanningForest forest(vertex_count);
    std::vector<dynamic_msf_test_detail::OracleEdge> edges;

    for (std::size_t step = 0U; step < 90U; ++step) {
      const bool should_add = edges.empty() || (random() % 100U) < 64U;
      if (should_add && edges.size() < 11U) {
        const std::size_t first =
            static_cast<std::size_t>(random() % vertex_count);
        const std::size_t second =
            static_cast<std::size_t>(random() % vertex_count);
        const std::int64_t weight =
            static_cast<std::int64_t>(random() % 31U) - 15;
        const auto handle = forest.add_edge(first, second, weight);
        REQUIRE_EQ(handle, edges.size());
        edges.push_back(
            dynamic_msf_test_detail::OracleEdge{first, second, weight, true});
      } else {
        std::vector<std::size_t> active;
        for (std::size_t index = 0U; index < edges.size(); ++index) {
          if (edges[index].active) {
            active.push_back(index);
          }
        }
        if (!active.empty()) {
          const std::size_t handle = active[static_cast<std::size_t>(
              random() % static_cast<std::uint64_t>(active.size()))];
          forest.remove_edge(handle);
          edges[handle].active = false;
        }
      }

      const auto expected =
          dynamic_msf_test_detail::exhaustive_optimum(vertex_count, edges);
      REQUIRE_EQ(forest.component_count(), expected.component_count);
      REQUIRE_EQ(forest.total_weight(), expected.total_weight);
      REQUIRE(forest.valid_structure());
    }
  }
}

TEST_CASE(dynamic_msf_empty_and_handle_contracts) {
  algorithms::graphs::DynamicMinimumSpanningForest empty(0U);
  REQUIRE_EQ(empty.component_count(), 0U);
  REQUIRE_EQ(empty.total_weight(), 0);
  REQUIRE(empty.valid_structure());
  REQUIRE_THROWS_AS(empty.add_edge(0U, 0U, 0), std::out_of_range);

  algorithms::graphs::DynamicMinimumSpanningForest singleton(1U);
  const auto loop = singleton.add_edge(0U, 0U, -7);
  REQUIRE(singleton.active(loop));
  REQUIRE(!singleton.in_forest(loop));
  REQUIRE_EQ(singleton.component_count(), 1U);
  REQUIRE_EQ(singleton.total_weight(), 0);
  singleton.remove_edge(loop);
  REQUIRE(!singleton.active(loop));
  REQUIRE(singleton.valid_structure());
}
