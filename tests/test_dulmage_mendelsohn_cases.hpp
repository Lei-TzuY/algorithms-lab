#pragma once

#include "algorithms/graphs/dulmage_mendelsohn.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dulmage_mendelsohn_test_detail {

using algorithms::graphs::BipartiteEdge;
using algorithms::graphs::DulmageMendelsohnRegion;

struct EnumeratedMatching {
  std::vector<std::optional<std::size_t>> left_match;
  std::vector<std::optional<std::size_t>> right_match;
  std::size_t cardinality{};
};

struct ExhaustiveOracle {
  std::size_t maximum_cardinality{};
  std::vector<DulmageMendelsohnRegion> left_region;
  std::vector<DulmageMendelsohnRegion> right_region;
  std::vector<std::optional<std::size_t>> left_block;
  std::vector<std::optional<std::size_t>> right_block;
  std::vector<std::vector<std::size_t>> block_left;
  std::vector<std::vector<std::size_t>> block_right;
  std::vector<std::pair<std::size_t, std::size_t>> block_dag_edges;
};

inline std::vector<EnumeratedMatching> enumerate_maximum_matchings(
    std::size_t left_count, std::size_t right_count,
    const std::vector<std::vector<bool>>& edge) {
  std::vector<EnumeratedMatching> best;
  std::size_t best_size = 0;
  std::vector<std::optional<std::size_t>> left_match(left_count);
  std::vector<std::optional<std::size_t>> right_match(right_count);
  std::vector<bool> used_right(right_count, false);

  std::function<void(std::size_t, std::size_t)> visit =
      [&](std::size_t left, std::size_t count) {
        if (left == left_count) {
          if (count > best_size) {
            best_size = count;
            best.clear();
          }
          if (count == best_size) {
            best.push_back(EnumeratedMatching{left_match, right_match, count});
          }
          return;
        }

        left_match[left].reset();
        visit(left + 1, count);
        for (std::size_t right = 0; right < right_count; ++right) {
          if (!edge[left][right] || used_right[right]) {
            continue;
          }
          used_right[right] = true;
          left_match[left] = right;
          right_match[right] = left;
          visit(left + 1, count + 1);
          right_match[right].reset();
          left_match[left].reset();
          used_right[right] = false;
        }
      };
  visit(0, 0);
  return best;
}

inline ExhaustiveOracle build_oracle(
    std::size_t left_count, std::size_t right_count,
    std::span<const BipartiteEdge> input_edges) {
  std::vector<std::vector<bool>> edge(
      left_count, std::vector<bool>(right_count, false));
  for (const BipartiteEdge& item : input_edges) {
    if (item.left >= left_count || item.right >= right_count) {
      throw std::out_of_range("oracle bipartite edge endpoint out of range");
    }
    edge[item.left][item.right] = true;
  }

  const std::vector<EnumeratedMatching> maximum_matchings =
      enumerate_maximum_matchings(left_count, right_count, edge);
  REQUIRE(!maximum_matchings.empty());
  const std::size_t maximum = maximum_matchings.front().cardinality;

  std::vector<bool> left_exposable(left_count, false);
  std::vector<bool> right_exposable(right_count, false);
  std::vector<std::vector<bool>> allowed(
      left_count, std::vector<bool>(right_count, false));
  for (const EnumeratedMatching& matching : maximum_matchings) {
    REQUIRE_EQ(matching.cardinality, maximum);
    for (std::size_t left = 0; left < left_count; ++left) {
      if (!matching.left_match[left].has_value()) {
        left_exposable[left] = true;
      } else {
        allowed[left][*matching.left_match[left]] = true;
      }
    }
    for (std::size_t right = 0; right < right_count; ++right) {
      if (!matching.right_match[right].has_value()) {
        right_exposable[right] = true;
      }
    }
  }

  const std::size_t total = left_count + right_count;
  std::vector<std::vector<std::size_t>> allowed_adjacency(total);
  for (std::size_t left = 0; left < left_count; ++left) {
    for (std::size_t right = 0; right < right_count; ++right) {
      if (!allowed[left][right]) {
        continue;
      }
      const std::size_t right_vertex = left_count + right;
      allowed_adjacency[left].push_back(right_vertex);
      allowed_adjacency[right_vertex].push_back(left);
    }
  }

  std::vector<std::size_t> component(total, total);
  std::vector<std::vector<std::size_t>> components;
  for (std::size_t start = 0; start < total; ++start) {
    if (component[start] != total) {
      continue;
    }
    const std::size_t component_id = components.size();
    components.emplace_back();
    std::queue<std::size_t> queue;
    queue.push(start);
    component[start] = component_id;
    while (!queue.empty()) {
      const std::size_t vertex = queue.front();
      queue.pop();
      components.back().push_back(vertex);
      for (const std::size_t next : allowed_adjacency[vertex]) {
        if (component[next] == total) {
          component[next] = component_id;
          queue.push(next);
        }
      }
    }
  }

  ExhaustiveOracle oracle;
  oracle.maximum_cardinality = maximum;
  oracle.left_region.assign(left_count, DulmageMendelsohnRegion::balanced);
  oracle.right_region.assign(right_count, DulmageMendelsohnRegion::balanced);
  oracle.left_block.resize(left_count);
  oracle.right_block.resize(right_count);

  std::vector<DulmageMendelsohnRegion> component_region(
      components.size(), DulmageMendelsohnRegion::balanced);
  for (std::size_t component_id = 0; component_id < components.size();
       ++component_id) {
    bool has_exposable_left = false;
    bool has_exposable_right = false;
    for (const std::size_t vertex : components[component_id]) {
      if (vertex < left_count) {
        has_exposable_left = has_exposable_left || left_exposable[vertex];
      } else {
        has_exposable_right =
            has_exposable_right || right_exposable[vertex - left_count];
      }
    }
    REQUIRE(!(has_exposable_left && has_exposable_right));
    if (has_exposable_left) {
      component_region[component_id] =
          DulmageMendelsohnRegion::left_deficient;
    } else if (has_exposable_right) {
      component_region[component_id] =
          DulmageMendelsohnRegion::right_deficient;
    }
  }

  for (std::size_t left = 0; left < left_count; ++left) {
    oracle.left_region[left] = component_region[component[left]];
  }
  for (std::size_t right = 0; right < right_count; ++right) {
    oracle.right_region[right] =
        component_region[component[left_count + right]];
  }

  std::vector<std::size_t> balanced_components;
  for (std::size_t component_id = 0; component_id < components.size();
       ++component_id) {
    if (component_region[component_id] == DulmageMendelsohnRegion::balanced) {
      balanced_components.push_back(component_id);
    }
  }
  std::sort(balanced_components.begin(), balanced_components.end(),
            [&](std::size_t left_component, std::size_t right_component) {
              return *std::min_element(components[left_component].begin(),
                                       components[left_component].end()) <
                     *std::min_element(components[right_component].begin(),
                                       components[right_component].end());
            });

  for (std::size_t block = 0; block < balanced_components.size(); ++block) {
    const std::size_t component_id = balanced_components[block];
    oracle.block_left.emplace_back();
    oracle.block_right.emplace_back();
    for (const std::size_t vertex : components[component_id]) {
      if (vertex < left_count) {
        oracle.left_block[vertex] = block;
        oracle.block_left.back().push_back(vertex);
      } else {
        const std::size_t right = vertex - left_count;
        oracle.right_block[right] = block;
        oracle.block_right.back().push_back(right);
      }
    }
    std::sort(oracle.block_left.back().begin(), oracle.block_left.back().end());
    std::sort(oracle.block_right.back().begin(), oracle.block_right.back().end());
  }

  std::set<std::pair<std::size_t, std::size_t>> dag_edges;
  for (std::size_t left = 0; left < left_count; ++left) {
    if (oracle.left_region[left] != DulmageMendelsohnRegion::balanced) {
      continue;
    }
    for (std::size_t right = 0; right < right_count; ++right) {
      if (!edge[left][right] ||
          oracle.right_region[right] != DulmageMendelsohnRegion::balanced) {
        continue;
      }
      const std::size_t left_block = *oracle.left_block[left];
      const std::size_t right_block = *oracle.right_block[right];
      if (left_block != right_block) {
        dag_edges.emplace(left_block, right_block);
      }
    }
  }
  oracle.block_dag_edges.assign(dag_edges.begin(), dag_edges.end());
  return oracle;
}

inline void verify_instance(std::size_t left_count, std::size_t right_count,
                            const std::vector<BipartiteEdge>& edges) {
  const auto result = algorithms::graphs::dulmage_mendelsohn_decomposition(
      left_count, right_count, edges);
  const ExhaustiveOracle oracle = build_oracle(left_count, right_count, edges);

  REQUIRE_EQ(result.matching.cardinality, oracle.maximum_cardinality);
  REQUIRE_EQ(result.left_region, oracle.left_region);
  REQUIRE_EQ(result.right_region, oracle.right_region);
  REQUIRE_EQ(result.left_balanced_block, oracle.left_block);
  REQUIRE_EQ(result.right_balanced_block, oracle.right_block);
  REQUIRE_EQ(result.balanced_blocks.size(), oracle.block_left.size());
  for (std::size_t block = 0; block < result.balanced_blocks.size(); ++block) {
    REQUIRE_EQ(result.balanced_blocks[block].left_vertices,
               oracle.block_left[block]);
    REQUIRE_EQ(result.balanced_blocks[block].right_vertices,
               oracle.block_right[block]);
  }
  REQUIRE_EQ(result.block_dag_edges, oracle.block_dag_edges);
}

}  // namespace dulmage_mendelsohn_test_detail

TEST_CASE(dulmage_mendelsohn_deterministic_regions_and_blocks) {
  using dulmage_mendelsohn_test_detail::verify_instance;
  verify_instance(0, 0, {});
  verify_instance(1, 1, {{0, 0}});
  verify_instance(2, 1, {{0, 0}, {1, 0}});
  verify_instance(1, 2, {{0, 0}, {0, 1}});
  verify_instance(2, 2, {{0, 0}, {1, 1}, {0, 1}});
  verify_instance(2, 2, {{0, 0}, {0, 1}, {1, 0}, {1, 1}});
  verify_instance(3, 3, {{0, 0}, {1, 1}, {2, 1}, {2, 2}});
  verify_instance(2, 2, {{0, 0}, {0, 0}, {1, 1}, {1, 1}, {0, 1}});
}

TEST_CASE(dulmage_mendelsohn_rejects_invalid_endpoint) {
  const std::array<algorithms::graphs::BipartiteEdge, 1> bad_left{{{1, 0}}};
  const std::array<algorithms::graphs::BipartiteEdge, 1> bad_right{{{0, 1}}};
  REQUIRE_THROWS_AS(
      algorithms::graphs::dulmage_mendelsohn_decomposition(1, 1, bad_left),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      algorithms::graphs::dulmage_mendelsohn_decomposition(1, 1, bad_right),
      std::out_of_range);
}

TEST_CASE(dulmage_mendelsohn_exhaustive_small_simple_graphs) {
  using dulmage_mendelsohn_test_detail::verify_instance;
  for (std::size_t left_count = 0; left_count <= 3; ++left_count) {
    for (std::size_t right_count = 0; right_count <= 3; ++right_count) {
      const std::size_t edge_slots = left_count * right_count;
      const std::uint64_t graph_count = std::uint64_t{1} << edge_slots;
      for (std::uint64_t mask = 0; mask < graph_count; ++mask) {
        std::vector<algorithms::graphs::BipartiteEdge> edges;
        for (std::size_t left = 0; left < left_count; ++left) {
          for (std::size_t right = 0; right < right_count; ++right) {
            const std::size_t bit = left * right_count + right;
            if (((mask >> bit) & std::uint64_t{1}) != 0) {
              edges.push_back(algorithms::graphs::BipartiteEdge{left, right});
            }
          }
        }
        verify_instance(left_count, right_count, edges);
      }
    }
  }
}

TEST_CASE(dulmage_mendelsohn_random_multigraphs_vs_all_maximum_matchings) {
  using dulmage_mendelsohn_test_detail::verify_instance;
  std::mt19937_64 rng(0xD01A6EULL);
  for (std::size_t trial = 0; trial < 500; ++trial) {
    const std::size_t left_count = static_cast<std::size_t>(rng() % 5);
    const std::size_t right_count = static_cast<std::size_t>(rng() % 5);
    std::vector<algorithms::graphs::BipartiteEdge> edges;
    for (std::size_t left = 0; left < left_count; ++left) {
      for (std::size_t right = 0; right < right_count; ++right) {
        if ((rng() % 100) < 38) {
          edges.push_back(algorithms::graphs::BipartiteEdge{left, right});
          if ((rng() % 5) == 0) {
            edges.push_back(algorithms::graphs::BipartiteEdge{left, right});
          }
        }
      }
    }
    verify_instance(left_count, right_count, edges);
  }
}
