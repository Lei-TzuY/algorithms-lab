#include "algorithms/graphs/traversal.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <stdexcept>
#include <utility>

namespace algorithms::graphs {

BreadthFirstSearchResult breadth_first_search(const Graph& graph, Vertex start) {
  graph.validate_vertex(start);
  BreadthFirstSearchResult result;
  result.distance.resize(graph.vertex_count());
  result.parent.resize(graph.vertex_count());

  std::deque<Vertex> queue;
  result.distance[start] = 0;
  queue.push_back(start);

  while (!queue.empty()) {
    const Vertex current = queue.front();
    queue.pop_front();
    result.order.push_back(current);

    for (const Edge& edge : graph.neighbors(current)) {
      if (result.distance[edge.to].has_value()) {
        continue;
      }
      result.distance[edge.to] = *result.distance[current] + 1;
      result.parent[edge.to] = current;
      queue.push_back(edge.to);
    }
  }
  return result;
}

std::optional<std::vector<Vertex>> shortest_unweighted_path(const Graph& graph,
                                                            Vertex start,
                                                            Vertex target) {
  graph.validate_vertex(target);
  const auto bfs = breadth_first_search(graph, start);
  if (!bfs.distance[target].has_value()) {
    return std::nullopt;
  }

  std::vector<Vertex> reversed;
  Vertex current = target;
  while (true) {
    reversed.push_back(current);
    if (current == start) {
      break;
    }
    current = *bfs.parent[current];
  }
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

std::vector<Vertex> depth_first_search(const Graph& graph, Vertex start) {
  graph.validate_vertex(start);
  std::vector<Vertex> order;
  std::vector<bool> visited(graph.vertex_count(), false);
  std::vector<Vertex> stack{start};

  while (!stack.empty()) {
    const Vertex current = stack.back();
    stack.pop_back();
    if (visited[current]) {
      continue;
    }
    visited[current] = true;
    order.push_back(current);

    const auto& neighbors = graph.neighbors(current);
    for (auto it = neighbors.rbegin(); it != neighbors.rend(); ++it) {
      if (!visited[it->to]) {
        stack.push_back(it->to);
      }
    }
  }
  return order;
}

std::vector<std::vector<Vertex>> connected_components(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "connected_components requires an undirected graph");
  }
  std::vector<std::vector<Vertex>> components;
  std::vector<bool> visited(graph.vertex_count(), false);

  for (Vertex start = 0; start < graph.vertex_count(); ++start) {
    if (visited[start]) {
      continue;
    }
    std::vector<Vertex> component;
    std::vector<Vertex> stack{start};
    while (!stack.empty()) {
      const Vertex current = stack.back();
      stack.pop_back();
      if (visited[current]) {
        continue;
      }
      visited[current] = true;
      component.push_back(current);
      const auto& neighbors = graph.neighbors(current);
      for (auto it = neighbors.rbegin(); it != neighbors.rend(); ++it) {
        if (!visited[it->to]) {
          stack.push_back(it->to);
        }
      }
    }
    components.push_back(std::move(component));
  }
  return components;
}

bool is_reachable(const Graph& graph, Vertex start, Vertex target) {
  graph.validate_vertex(target);
  const auto bfs = breadth_first_search(graph, start);
  return bfs.distance[target].has_value();
}

namespace {

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

bool directed_cycle_from(const Graph& graph, Vertex vertex,
                         std::vector<unsigned char>& color) {
  color[vertex] = 1;
  for (const Edge& edge : graph.neighbors(vertex)) {
    if (color[edge.to] == 1) {
      return true;
    }
    if (color[edge.to] == 0 && directed_cycle_from(graph, edge.to, color)) {
      return true;
    }
  }
  color[vertex] = 2;
  return false;
}

bool undirected_cycle_from(const Graph& graph, Vertex vertex,
                           std::optional<Vertex> parent,
                           std::vector<bool>& visited) {
  visited[vertex] = true;
  std::size_t edges_to_parent = 0;
  for (const Edge& edge : graph.neighbors(vertex)) {
    if (edge.to == vertex) {
      return true;
    }
    if (parent.has_value() && edge.to == *parent) {
      ++edges_to_parent;
      if (edges_to_parent > 1) {
        return true;  // parallel edge to parent forms a 2-edge cycle
      }
      continue;
    }
    if (visited[edge.to]) {
      return true;
    }
    if (undirected_cycle_from(graph, edge.to, vertex, visited)) {
      return true;
    }
  }
  return false;
}

struct IndexedUndirectedEdge {
  Vertex first;
  Vertex second;
  Weight weight;
};

struct LowLinkAdjacencyEntry {
  Vertex to;
  std::size_t edge_id;
};

struct LowLinkDfsFrame {
  Vertex vertex;
  std::size_t next_neighbor = 0;
};

std::vector<IndexedUndirectedEdge> extract_undirected_edges(const Graph& graph) {
  std::vector<IndexedUndirectedEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (from <= edge.to) {
        edges.push_back(IndexedUndirectedEdge{from, edge.to, edge.weight});
      }
    }
  }
  return edges;
}

}  // namespace

bool has_cycle(const Graph& graph) {
  if (graph.directed()) {
    std::vector<unsigned char> color(graph.vertex_count(), 0);
    for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
      if (color[vertex] == 0 && directed_cycle_from(graph, vertex, color)) {
        return true;
      }
    }
    return false;
  }

  std::vector<bool> visited(graph.vertex_count(), false);
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (!visited[vertex] &&
        undirected_cycle_from(graph, vertex, std::nullopt, visited)) {
      return true;
    }
  }
  return false;
}

UndirectedLowLinkResult analyze_undirected_low_link(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "analyze_undirected_low_link requires an undirected graph");
  }

  const std::size_t vertex_count = graph.vertex_count();
  const std::vector<IndexedUndirectedEdge> edges =
      extract_undirected_edges(graph);

  std::vector<std::vector<LowLinkAdjacencyEntry>> adjacency(vertex_count);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    const IndexedUndirectedEdge& edge = edges[edge_id];
    adjacency[edge.first].push_back(
        LowLinkAdjacencyEntry{edge.second, edge_id});
    if (edge.first != edge.second) {
      adjacency[edge.second].push_back(
          LowLinkAdjacencyEntry{edge.first, edge_id});
    }
  }

  std::vector<std::size_t> discovery(vertex_count, kNoIndex);
  std::vector<std::size_t> low(vertex_count, kNoIndex);
  std::vector<Vertex> parent_vertex(vertex_count, kNoIndex);
  std::vector<std::size_t> parent_edge(vertex_count, kNoIndex);
  std::vector<std::size_t> child_count(vertex_count, 0);
  std::vector<bool> articulation(vertex_count, false);
  std::vector<bool> is_bridge(edges.size(), false);
  std::size_t timer = 0;

  for (Vertex root = 0; root < vertex_count; ++root) {
    if (discovery[root] != kNoIndex) {
      continue;
    }

    discovery[root] = timer;
    low[root] = timer;
    ++timer;
    std::vector<LowLinkDfsFrame> stack{{root, 0}};

    while (!stack.empty()) {
      LowLinkDfsFrame& frame = stack.back();
      const Vertex vertex = frame.vertex;
      if (frame.next_neighbor < adjacency[vertex].size()) {
        const LowLinkAdjacencyEntry entry =
            adjacency[vertex][frame.next_neighbor];
        ++frame.next_neighbor;

        if (entry.edge_id == parent_edge[vertex]) {
          continue;
        }
        if (discovery[entry.to] == kNoIndex) {
          parent_vertex[entry.to] = vertex;
          parent_edge[entry.to] = entry.edge_id;
          ++child_count[vertex];
          discovery[entry.to] = timer;
          low[entry.to] = timer;
          ++timer;
          stack.push_back(LowLinkDfsFrame{entry.to, 0});
          continue;
        }
        low[vertex] = std::min(low[vertex], discovery[entry.to]);
        continue;
      }

      stack.pop_back();
      if (parent_edge[vertex] == kNoIndex) {
        articulation[vertex] = child_count[vertex] > 1;
        continue;
      }

      const Vertex parent = parent_vertex[vertex];
      low[parent] = std::min(low[parent], low[vertex]);
      if (low[vertex] > discovery[parent]) {
        is_bridge[parent_edge[vertex]] = true;
      }
      if (parent_edge[parent] != kNoIndex &&
          low[vertex] >= discovery[parent]) {
        articulation[parent] = true;
      }
    }
  }

  UndirectedLowLinkResult result;
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    if (is_bridge[edge_id]) {
      const IndexedUndirectedEdge& edge = edges[edge_id];
      result.bridges.push_back(
          UndirectedEdgeWitness{edge.first, edge.second, edge.weight});
    }
  }
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (articulation[vertex]) {
      result.articulation_vertices.push_back(vertex);
    }
  }

  result.bridge_component_of.assign(vertex_count, kNoIndex);
  for (Vertex seed = 0; seed < vertex_count; ++seed) {
    if (result.bridge_component_of[seed] != kNoIndex) {
      continue;
    }

    const std::size_t component = result.bridge_component_count;
    ++result.bridge_component_count;
    result.bridge_component_of[seed] = component;
    std::vector<Vertex> stack{seed};
    while (!stack.empty()) {
      const Vertex vertex = stack.back();
      stack.pop_back();
      for (const LowLinkAdjacencyEntry entry : adjacency[vertex]) {
        if (is_bridge[entry.edge_id] ||
            result.bridge_component_of[entry.to] != kNoIndex) {
          continue;
        }
        result.bridge_component_of[entry.to] = component;
        stack.push_back(entry.to);
      }
    }
  }

  return result;
}

std::optional<std::vector<Vertex>> topological_sort(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument("topological_sort requires a directed graph");
  }

  std::vector<std::size_t> indegree(graph.vertex_count(), 0);
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      ++indegree[edge.to];
    }
  }

  std::deque<Vertex> ready;
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (indegree[vertex] == 0) {
      ready.push_back(vertex);
    }
  }

  std::vector<Vertex> order;
  while (!ready.empty()) {
    const Vertex current = ready.front();
    ready.pop_front();
    order.push_back(current);
    for (const Edge& edge : graph.neighbors(current)) {
      --indegree[edge.to];
      if (indegree[edge.to] == 0) {
        ready.push_back(edge.to);
      }
    }
  }

  if (order.size() != graph.vertex_count()) {
    return std::nullopt;
  }
  return order;
}

}  // namespace algorithms::graphs
