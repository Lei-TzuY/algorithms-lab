#include "algorithms/graphs/strongly_connected_components.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

void require_directed(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument(
        "strongly connected components require a directed graph");
  }
}

}  // namespace

StronglyConnectedComponents tarjan_strongly_connected_components(
    const Graph& graph) {
  require_directed(graph);

  const std::size_t vertex_count = graph.vertex_count();
  const std::size_t unvisited = vertex_count;
  std::vector<std::size_t> index(vertex_count, unvisited);
  std::vector<std::size_t> lowlink(vertex_count, unvisited);
  std::vector<Vertex> stack;
  std::vector<bool> on_stack(vertex_count, false);
  std::size_t next_index = 0;

  StronglyConnectedComponents result;
  result.component_of.resize(vertex_count, unvisited);

  std::function<void(Vertex)> visit = [&](Vertex vertex) {
    index[vertex] = next_index;
    lowlink[vertex] = next_index;
    ++next_index;
    stack.push_back(vertex);
    on_stack[vertex] = true;

    for (const Edge& edge : graph.neighbors(vertex)) {
      if (index[edge.to] == unvisited) {
        visit(edge.to);
        lowlink[vertex] = std::min(lowlink[vertex], lowlink[edge.to]);
      } else if (on_stack[edge.to]) {
        lowlink[vertex] = std::min(lowlink[vertex], index[edge.to]);
      }
    }

    if (lowlink[vertex] != index[vertex]) {
      return;
    }

    const std::size_t component_id = result.components.size();
    result.components.emplace_back();
    while (true) {
      const Vertex member = stack.back();
      stack.pop_back();
      on_stack[member] = false;
      result.component_of[member] = component_id;
      result.components.back().push_back(member);
      if (member == vertex) {
        break;
      }
    }
  };

  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (index[vertex] == unvisited) {
      visit(vertex);
    }
  }

  return result;
}

StronglyConnectedComponents kosaraju_strongly_connected_components(
    const Graph& graph) {
  require_directed(graph);

  const std::size_t vertex_count = graph.vertex_count();
  std::vector<bool> visited(vertex_count, false);
  std::vector<Vertex> finish_order;
  finish_order.reserve(vertex_count);

  std::function<void(Vertex)> first_pass = [&](Vertex vertex) {
    visited[vertex] = true;
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (!visited[edge.to]) {
        first_pass(edge.to);
      }
    }
    finish_order.push_back(vertex);
  };

  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (!visited[vertex]) {
      first_pass(vertex);
    }
  }

  std::vector<std::vector<Vertex>> transpose(vertex_count);
  for (Vertex from = 0; from < vertex_count; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      transpose[edge.to].push_back(from);
    }
  }

  StronglyConnectedComponents result;
  result.component_of.resize(vertex_count, vertex_count);
  std::fill(visited.begin(), visited.end(), false);

  std::function<void(Vertex, std::size_t)> second_pass =
      [&](Vertex vertex, std::size_t component_id) {
        visited[vertex] = true;
        result.component_of[vertex] = component_id;
        result.components[component_id].push_back(vertex);
        for (const Vertex next : transpose[vertex]) {
          if (!visited[next]) {
            second_pass(next, component_id);
          }
        }
      };

  for (auto it = finish_order.rbegin(); it != finish_order.rend(); ++it) {
    if (visited[*it]) {
      continue;
    }
    const std::size_t component_id = result.components.size();
    result.components.emplace_back();
    second_pass(*it, component_id);
  }

  return result;
}

Graph condensation_graph(
    const Graph& graph, const StronglyConnectedComponents& decomposition) {
  require_directed(graph);
  if (decomposition.component_of.size() != graph.vertex_count()) {
    throw std::invalid_argument("SCC decomposition vertex count mismatch");
  }

  const std::size_t component_count = decomposition.components.size();
  for (const std::size_t component_id : decomposition.component_of) {
    if (component_id >= component_count) {
      throw std::invalid_argument("SCC decomposition has invalid component ID");
    }
  }

  std::set<std::pair<Vertex, Vertex>> inter_component_edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const Vertex component_from = decomposition.component_of[from];
    for (const Edge& edge : graph.neighbors(from)) {
      const Vertex component_to = decomposition.component_of[edge.to];
      if (component_from != component_to) {
        inter_component_edges.emplace(component_from, component_to);
      }
    }
  }

  Graph result(component_count, true);
  for (const auto& [from, to] : inter_component_edges) {
    result.add_edge(from, to);
  }
  return result;
}

}  // namespace algorithms::graphs
