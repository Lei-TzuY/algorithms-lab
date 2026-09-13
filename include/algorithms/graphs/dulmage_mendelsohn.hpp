#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/bipartite_matching.hpp"
#include "algorithms/graphs/strongly_connected_components.hpp"

namespace algorithms::graphs {

enum class DulmageMendelsohnRegion {
  left_deficient,
  balanced,
  right_deficient,
};

struct DulmageMendelsohnBlock {
  std::vector<std::size_t> left_vertices;
  std::vector<std::size_t> right_vertices;
};

struct DulmageMendelsohnDecomposition {
  BipartiteMatchingResult matching;
  std::vector<DulmageMendelsohnRegion> left_region;
  std::vector<DulmageMendelsohnRegion> right_region;
  std::vector<std::optional<std::size_t>> left_balanced_block;
  std::vector<std::optional<std::size_t>> right_balanced_block;
  std::vector<DulmageMendelsohnBlock> balanced_blocks;
  std::vector<std::pair<std::size_t, std::size_t>> block_dag_edges;
};

namespace detail {

inline std::vector<bool> dm_reachable(
    const std::vector<std::vector<std::size_t>>& adjacency,
    const std::vector<std::size_t>& sources) {
  std::vector<bool> seen(adjacency.size(), false);
  std::queue<std::size_t> queue;
  for (const std::size_t source : sources) {
    if (!seen[source]) {
      seen[source] = true;
      queue.push(source);
    }
  }
  while (!queue.empty()) {
    const std::size_t vertex = queue.front();
    queue.pop();
    for (const std::size_t next : adjacency[vertex]) {
      if (!seen[next]) {
        seen[next] = true;
        queue.push(next);
      }
    }
  }
  return seen;
}

}  // namespace detail

// Coarse/fine Dulmage-Mendelsohn decomposition of a bipartite graph.
// Parallel copies collapse to one endpoint pair because vertex matching semantics
// do not distinguish copies; edge order otherwise has no semantic role.
//
// The alternating digraph orients every unmatched simple edge L->R and every
// matched edge R->L. Vertices reachable from a free left vertex form the
// left-deficient coarse region; vertices that can reach a free right vertex form
// the right-deficient coarse region. Maximum matching excludes overlap between
// those regions. Every remaining vertex is matched. Contracting each remaining
// matched pair and taking SCCs of the unmatched-edge digraph yields the balanced
// DM blocks; the returned DAG is the SCC condensation restricted to those blocks.
//
// With V=L+R and E input edge copies, this wrapper adds O(E log(E+1)+V+E)
// work around the sealed Hopcroft-Karp O(E*sqrt(V+1)) matching call and the
// sealed linear-time SCC pass. Auxiliary/output storage is O(V+E).
[[nodiscard]] inline DulmageMendelsohnDecomposition dulmage_mendelsohn_decomposition(
    std::size_t left_count, std::size_t right_count,
    std::span<const BipartiteEdge> edges) {
  if (left_count > std::numeric_limits<std::size_t>::max() - right_count) {
    throw std::length_error("Dulmage-Mendelsohn vertex count overflow");
  }
  const std::size_t total_vertices = left_count + right_count;

  std::set<std::pair<std::size_t, std::size_t>> simple_edges;
  for (const BipartiteEdge& edge : edges) {
    if (edge.left >= left_count || edge.right >= right_count) {
      throw std::out_of_range("bipartite edge endpoint is out of range");
    }
    simple_edges.emplace(edge.left, edge.right);
  }

  DulmageMendelsohnDecomposition result;
  result.matching = hopcroft_karp(left_count, right_count, edges);
  result.left_region.assign(left_count, DulmageMendelsohnRegion::balanced);
  result.right_region.assign(right_count, DulmageMendelsohnRegion::balanced);
  result.left_balanced_block.resize(left_count);
  result.right_balanced_block.resize(right_count);

  std::vector<std::vector<std::size_t>> alternating(total_vertices);
  std::vector<std::vector<std::size_t>> reverse(total_vertices);
  const auto right_vertex = [left_count](std::size_t right) {
    return left_count + right;
  };
  for (const auto& [left, right] : simple_edges) {
    const std::size_t l = left;
    const std::size_t r = right_vertex(right);
    if (result.matching.left_match[left].has_value() &&
        *result.matching.left_match[left] == right) {
      alternating[r].push_back(l);
      reverse[l].push_back(r);
    } else {
      alternating[l].push_back(r);
      reverse[r].push_back(l);
    }
  }

  std::vector<std::size_t> free_left;
  for (std::size_t left = 0; left < left_count; ++left) {
    if (!result.matching.left_match[left].has_value()) free_left.push_back(left);
  }
  std::vector<std::size_t> free_right;
  for (std::size_t right = 0; right < right_count; ++right) {
    if (!result.matching.right_match[right].has_value()) {
      free_right.push_back(right_vertex(right));
    }
  }

  const std::vector<bool> from_free_left =
      detail::dm_reachable(alternating, free_left);
  const std::vector<bool> to_free_right =
      detail::dm_reachable(reverse, free_right);
  for (std::size_t vertex = 0; vertex < total_vertices; ++vertex) {
    if (from_free_left[vertex] && to_free_right[vertex]) {
      throw std::logic_error("maximum matching admits an augmenting path");
    }
  }
  for (std::size_t left = 0; left < left_count; ++left) {
    if (from_free_left[left]) {
      result.left_region[left] = DulmageMendelsohnRegion::left_deficient;
    } else if (to_free_right[left]) {
      result.left_region[left] = DulmageMendelsohnRegion::right_deficient;
    }
  }
  for (std::size_t right = 0; right < right_count; ++right) {
    const std::size_t vertex = right_vertex(right);
    if (from_free_left[vertex]) {
      result.right_region[right] = DulmageMendelsohnRegion::left_deficient;
    } else if (to_free_right[vertex]) {
      result.right_region[right] = DulmageMendelsohnRegion::right_deficient;
    }
  }

  const std::size_t no_pair = left_count;
  std::vector<std::size_t> left_pair(left_count, no_pair);
  std::vector<std::size_t> right_pair(right_count, no_pair);
  std::vector<std::pair<std::size_t, std::size_t>> pairs;
  for (std::size_t left = 0; left < left_count; ++left) {
    if (result.left_region[left] != DulmageMendelsohnRegion::balanced) continue;
    if (!result.matching.left_match[left].has_value()) {
      throw std::logic_error("balanced left vertex is unmatched");
    }
    const std::size_t right = *result.matching.left_match[left];
    if (right >= right_count ||
        result.right_region[right] != DulmageMendelsohnRegion::balanced) {
      throw std::logic_error("balanced matched pair crosses a DM region");
    }
    const std::size_t pair_id = pairs.size();
    left_pair[left] = pair_id;
    if (right_pair[right] != no_pair) {
      throw std::logic_error("matching is not one-to-one");
    }
    right_pair[right] = pair_id;
    pairs.emplace_back(left, right);
  }
  for (std::size_t right = 0; right < right_count; ++right) {
    if (result.right_region[right] == DulmageMendelsohnRegion::balanced &&
        right_pair[right] == no_pair) {
      throw std::logic_error("balanced right vertex is unmatched");
    }
  }

  Graph pair_graph(pairs.size(), true);
  for (const auto& [left, right] : simple_edges) {
    if (result.left_region[left] != DulmageMendelsohnRegion::balanced ||
        result.right_region[right] != DulmageMendelsohnRegion::balanced) {
      continue;
    }
    if (result.matching.left_match[left].has_value() &&
        *result.matching.left_match[left] == right) {
      continue;
    }
    pair_graph.add_edge(left_pair[left], right_pair[right]);
  }

  const StronglyConnectedComponents scc =
      tarjan_strongly_connected_components(pair_graph);
  const std::vector<std::size_t>& component_of = scc.component_of;
  const std::size_t component_count = scc.components.size();
  std::vector<std::vector<std::size_t>> component_pairs(component_count);
  for (std::size_t pair_id = 0; pair_id < pairs.size(); ++pair_id) {
    component_pairs[component_of[pair_id]].push_back(pair_id);
  }
  std::vector<std::size_t> component_order(component_count);
  for (std::size_t component = 0; component < component_count; ++component) {
    component_order[component] = component;
  }
  const auto component_key = [&](std::size_t component) {
    std::size_t key = total_vertices;
    for (const std::size_t pair_id : component_pairs[component]) {
      key = std::min(key, pairs[pair_id].first);
      key = std::min(key, right_vertex(pairs[pair_id].second));
    }
    return key;
  };
  std::sort(component_order.begin(), component_order.end(),
            [&](std::size_t a, std::size_t b) {
              return component_key(a) < component_key(b);
            });
  std::vector<std::size_t> block_of_component(component_count);
  result.balanced_blocks.resize(component_count);
  for (std::size_t block = 0; block < component_order.size(); ++block) {
    const std::size_t component = component_order[block];
    block_of_component[component] = block;
    auto& output = result.balanced_blocks[block];
    for (const std::size_t pair_id : component_pairs[component]) {
      output.left_vertices.push_back(pairs[pair_id].first);
      output.right_vertices.push_back(pairs[pair_id].second);
    }
    std::sort(output.left_vertices.begin(), output.left_vertices.end());
    std::sort(output.right_vertices.begin(), output.right_vertices.end());
  }
  for (std::size_t pair_id = 0; pair_id < pairs.size(); ++pair_id) {
    const std::size_t block = block_of_component[component_of[pair_id]];
    result.left_balanced_block[pairs[pair_id].first] = block;
    result.right_balanced_block[pairs[pair_id].second] = block;
  }

  std::set<std::pair<std::size_t, std::size_t>> dag_edges;
  for (std::size_t from = 0; from < pair_graph.vertex_count(); ++from) {
    const std::size_t from_block = block_of_component[component_of[from]];
    for (const Edge& edge : pair_graph.neighbors(from)) {
      const std::size_t to_block = block_of_component[component_of[edge.to]];
      if (from_block != to_block) dag_edges.emplace(from_block, to_block);
    }
  }
  result.block_dag_edges.assign(dag_edges.begin(), dag_edges.end());
  return result;
}

}  // namespace algorithms::graphs
