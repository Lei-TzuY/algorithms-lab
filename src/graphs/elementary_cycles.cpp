#include "algorithms/graphs/elementary_cycles.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/strongly_connected_components.hpp"

namespace algorithms::graphs {
namespace {

std::vector<std::vector<Vertex>> canonical_adjacency(const Graph& graph) {
  std::vector<std::vector<Vertex>> adjacency(graph.vertex_count());
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    auto& row = adjacency[from];
    row.reserve(graph.neighbors(from).size());
    for (const Edge& edge : graph.neighbors(from)) row.push_back(edge.to);
    std::sort(row.begin(), row.end());
    row.erase(std::unique(row.begin(), row.end()), row.end());
  }
  return adjacency;
}

bool contains_self_loop(const std::vector<std::vector<Vertex>>& adjacency,
                        Vertex vertex) {
  const auto& row = adjacency[vertex];
  return std::binary_search(row.begin(), row.end(), vertex);
}

}  // namespace

std::vector<std::vector<Vertex>> johnson_elementary_cycles(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument(
        "elementary cycle enumeration requires a directed graph");
  }

  const std::size_t n = graph.vertex_count();
  const auto adjacency = canonical_adjacency(graph);
  std::vector<std::vector<Vertex>> cycles;

  Vertex lower = 0;
  while (lower < n) {
    Graph suffix_graph(n, true);
    for (Vertex from = lower; from < n; ++from) {
      for (const Vertex to : adjacency[from]) {
        if (to >= lower) suffix_graph.add_edge(from, to);
      }
    }

    const auto scc = tarjan_strongly_connected_components(suffix_graph);
    std::size_t chosen_component = scc.components.size();
    Vertex start = n;

    for (std::size_t component_id = 0; component_id < scc.components.size();
         ++component_id) {
      const auto& component = scc.components[component_id];
      Vertex minimum = n;
      std::size_t relevant_size = 0;
      for (const Vertex vertex : component) {
        if (vertex < lower) continue;
        minimum = std::min(minimum, vertex);
        ++relevant_size;
      }
      if (relevant_size == 0) continue;
      const bool cyclic =
          relevant_size > 1 || contains_self_loop(adjacency, minimum);
      if (cyclic && minimum < start) {
        start = minimum;
        chosen_component = component_id;
      }
    }

    if (chosen_component == scc.components.size()) break;

    std::vector<bool> allowed(n, false);
    for (const Vertex vertex : scc.components[chosen_component]) {
      if (vertex >= lower) allowed[vertex] = true;
    }

    std::vector<bool> blocked(n, false);
    std::vector<std::vector<Vertex>> blocked_by(n);
    std::vector<Vertex> stack;

    std::function<void(Vertex)> unblock = [&](Vertex vertex) {
      blocked[vertex] = false;
      auto dependents = std::move(blocked_by[vertex]);
      blocked_by[vertex].clear();
      for (const Vertex dependent : dependents) {
        if (blocked[dependent]) unblock(dependent);
      }
    };

    std::function<bool(Vertex)> circuit = [&](Vertex vertex) {
      bool found_cycle = false;
      stack.push_back(vertex);
      blocked[vertex] = true;

      for (const Vertex next : adjacency[vertex]) {
        if (!allowed[next]) continue;
        if (next == start) {
          cycles.push_back(stack);
          found_cycle = true;
        } else if (!blocked[next] && circuit(next)) {
          found_cycle = true;
        }
      }

      if (found_cycle) {
        unblock(vertex);
      } else {
        for (const Vertex next : adjacency[vertex]) {
          if (!allowed[next]) continue;
          auto& dependents = blocked_by[next];
          if (std::find(dependents.begin(), dependents.end(), vertex) ==
              dependents.end()) {
            dependents.push_back(vertex);
          }
        }
      }

      stack.pop_back();
      return found_cycle;
    };

    static_cast<void>(circuit(start));
    lower = start + 1;
  }

  std::sort(cycles.begin(), cycles.end());
  cycles.erase(std::unique(cycles.begin(), cycles.end()), cycles.end());
  return cycles;
}

}  // namespace algorithms::graphs
