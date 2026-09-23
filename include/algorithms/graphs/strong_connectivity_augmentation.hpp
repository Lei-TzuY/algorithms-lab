#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/strongly_connected_components.hpp"

namespace algorithms::graphs {

struct StrongConnectivityAugmentation {
  // Directed edges to add to the original graph.
  // Endpoints are original graph vertices.
  std::vector<std::pair<Vertex, Vertex>> added_edges;
};

// Returns a minimum-cardinality set of directed edges whose addition makes the
// graph strongly connected.
//
// The graph must be directed. Empty graphs and graphs already consisting of one
// SCC need no added edges.
//
// For a condensation DAG with at least two SCCs, let S be the number of source
// components and T the number of sink components. Every strong augmentation
// needs at least max(S,T) edges: every source needs a new incoming edge and
// every sink needs a new outgoing edge.
//
// Construction:
//   1. Build the reachability bipartite graph source-SCC -> sink-SCC.
//   2. Find a maximum matching.
//   3. Cycle the matched source/sink pairs.
//   4. Pair unmatched sinks to unmatched sources; attach any surplus endpoints
//      directly to the matched core.
//
// Maximum matching guarantees that an unmatched source reaches only matched
// sinks and an unmatched sink is reachable only from matched sources. Therefore
// every unmatched endpoint can be attached to the matched strongly-connected
// core with one added edge, meeting the lower bound exactly.
//
// Conservative time bound:
//   O(E log E + S * (C + E_c) + S * S * T + V)
// where C/E_c are condensation vertices/edges; the E log E term comes from
// deterministic duplicate removal in condensation_graph(). Space is
// O(V + E_c + S*T).
[[nodiscard]] inline StrongConnectivityAugmentation
minimum_strong_connectivity_augmentation(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument(
        "strong-connectivity augmentation requires a directed graph");
  }

  const StronglyConnectedComponents scc =
      tarjan_strongly_connected_components(graph);
  const std::size_t component_count = scc.components.size();

  StrongConnectivityAugmentation result;
  if (component_count <= 1U) {
    return result;
  }

  const Graph dag = condensation_graph(graph, scc);

  std::vector<Vertex> representative(component_count);
  for (std::size_t component = 0U;
       component < component_count; ++component) {
    if (scc.components[component].empty()) {
      throw std::logic_error(
          "SCC decomposition contains an empty component");
    }
    representative[component] =
        *std::min_element(scc.components[component].begin(),
                          scc.components[component].end());
  }

  std::vector<std::size_t> indegree(component_count, 0U);
  std::vector<std::size_t> outdegree(component_count, 0U);
  for (Vertex from = 0U; from < component_count; ++from) {
    for (const Edge& edge : dag.neighbors(from)) {
      ++outdegree[from];
      ++indegree[edge.to];
    }
  }

  std::vector<std::size_t> sources;
  std::vector<std::size_t> sinks;
  for (std::size_t component = 0U;
       component < component_count; ++component) {
    if (indegree[component] == 0U) {
      sources.push_back(component);
    }
    if (outdegree[component] == 0U) {
      sinks.push_back(component);
    }
  }

  const auto by_representative =
      [&](const std::size_t left, const std::size_t right) {
        return representative[left] < representative[right];
      };
  std::sort(sources.begin(), sources.end(), by_representative);
  std::sort(sinks.begin(), sinks.end(), by_representative);

  if (sources.empty() || sinks.empty()) {
    throw std::logic_error(
        "nontrivial condensation DAG must have sources and sinks");
  }

  // reachable[source_index][sink_index]
  std::vector<std::vector<bool>> reachable(
      sources.size(), std::vector<bool>(sinks.size(), false));

  for (std::size_t source_index = 0U;
       source_index < sources.size(); ++source_index) {
    std::vector<bool> visited(component_count, false);
    std::vector<Vertex> stack;
    stack.push_back(sources[source_index]);
    visited[sources[source_index]] = true;

    while (!stack.empty()) {
      const Vertex current = stack.back();
      stack.pop_back();
      for (const Edge& edge : dag.neighbors(current)) {
        if (!visited[edge.to]) {
          visited[edge.to] = true;
          stack.push_back(edge.to);
        }
      }
    }

    for (std::size_t sink_index = 0U;
         sink_index < sinks.size(); ++sink_index) {
      reachable[source_index][sink_index] =
          visited[sinks[sink_index]];
    }
  }

  const std::size_t no_match =
      std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> sink_match(sinks.size(), no_match);

  const auto augment_from =
      [&](auto&& self, const std::size_t source_index,
          std::vector<bool>& seen_sinks) -> bool {
        for (std::size_t sink_index = 0U;
             sink_index < sinks.size(); ++sink_index) {
          if (!reachable[source_index][sink_index] ||
              seen_sinks[sink_index]) {
            continue;
          }
          seen_sinks[sink_index] = true;

          if (sink_match[sink_index] == no_match ||
              self(self, sink_match[sink_index], seen_sinks)) {
            sink_match[sink_index] = source_index;
            return true;
          }
        }
        return false;
      };

  for (std::size_t source_index = 0U;
       source_index < sources.size(); ++source_index) {
    std::vector<bool> seen_sinks(sinks.size(), false);
    (void)augment_from(augment_from, source_index, seen_sinks);
  }

  std::vector<std::size_t> source_match(sources.size(), no_match);
  for (std::size_t sink_index = 0U;
       sink_index < sinks.size(); ++sink_index) {
    if (sink_match[sink_index] != no_match) {
      source_match[sink_match[sink_index]] = sink_index;
    }
  }

  std::vector<std::pair<std::size_t, std::size_t>> matched_pairs;
  std::vector<std::size_t> unmatched_sources;
  std::vector<std::size_t> unmatched_sinks;

  for (std::size_t source_index = 0U;
       source_index < sources.size(); ++source_index) {
    if (source_match[source_index] == no_match) {
      unmatched_sources.push_back(source_index);
    } else {
      matched_pairs.emplace_back(source_index,
                                 source_match[source_index]);
    }
  }
  for (std::size_t sink_index = 0U;
       sink_index < sinks.size(); ++sink_index) {
    if (sink_match[sink_index] == no_match) {
      unmatched_sinks.push_back(sink_index);
    }
  }

  if (matched_pairs.empty()) {
    throw std::logic_error(
        "source-to-sink reachability matching must be nonempty");
  }

  result.added_edges.reserve(
      std::max(sources.size(), sinks.size()));

  // Each matched source reaches its matched sink in the condensation DAG.
  // Cycling matched sinks back to the next matched source makes the matched
  // endpoints one strongly-connected core.
  for (std::size_t index = 0U;
       index < matched_pairs.size(); ++index) {
    const std::size_t sink_index =
        matched_pairs[index].second;
    const std::size_t next_source_index =
        matched_pairs[(index + 1U) % matched_pairs.size()].first;

    result.added_edges.emplace_back(
        representative[sinks[sink_index]],
        representative[sources[next_source_index]]);
  }

  const std::size_t paired_unmatched =
      std::min(unmatched_sources.size(), unmatched_sinks.size());

  for (std::size_t index = 0U;
       index < paired_unmatched; ++index) {
    result.added_edges.emplace_back(
        representative[sinks[unmatched_sinks[index]]],
        representative[sources[unmatched_sources[index]]]);
  }

  const std::size_t anchor_source_index =
      matched_pairs.front().first;
  const std::size_t anchor_sink_index =
      matched_pairs.front().second;

  for (std::size_t index = paired_unmatched;
       index < unmatched_sources.size(); ++index) {
    result.added_edges.emplace_back(
        representative[sinks[anchor_sink_index]],
        representative[sources[unmatched_sources[index]]]);
  }

  for (std::size_t index = paired_unmatched;
       index < unmatched_sinks.size(); ++index) {
    result.added_edges.emplace_back(
        representative[sinks[unmatched_sinks[index]]],
        representative[sources[anchor_source_index]]);
  }

  if (result.added_edges.size() !=
      std::max(sources.size(), sinks.size())) {
    throw std::logic_error(
        "strong-connectivity augmentation missed the exact lower bound");
  }

  return result;
}

}  // namespace algorithms::graphs
