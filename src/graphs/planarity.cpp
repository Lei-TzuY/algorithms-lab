#include "algorithms/graphs/planarity.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

void increment_checked(std::size_t& value) {
  if (value == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error("planarity diagnostic arithmetic overflow");
  }
  ++value;
}

struct SimpleGraph {
  std::vector<std::vector<Vertex>> neighbors;
};

SimpleGraph simplify(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from != edge.to) {
        adjacent[from][edge.to] = true;
        adjacent[edge.to][from] = true;
      }
    }
  }

  SimpleGraph simple;
  simple.neighbors.resize(n);
  for (Vertex from = 0; from < n; ++from) {
    for (Vertex to = from + 1; to < n; ++to) {
      if (adjacent[from][to]) {
        simple.neighbors[from].push_back(to);
        simple.neighbors[to].push_back(from);
      }
    }
  }
  return simple;
}

std::vector<std::vector<Vertex>> components_of(const SimpleGraph& graph) {
  const std::size_t n = graph.neighbors.size();
  std::vector<bool> seen(n, false);
  std::vector<std::vector<Vertex>> components;
  for (Vertex start = 0; start < n; ++start) {
    if (seen[start]) {
      continue;
    }
    std::vector<Vertex> component;
    std::vector<Vertex> stack{start};
    seen[start] = true;
    while (!stack.empty()) {
      const Vertex vertex = stack.back();
      stack.pop_back();
      component.push_back(vertex);
      for (const Vertex next : graph.neighbors[vertex]) {
        if (!seen[next]) {
          seen[next] = true;
          stack.push_back(next);
        }
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  }
  return components;
}

std::size_t component_edge_count(const SimpleGraph& graph,
                                 const std::vector<Vertex>& component) {
  std::size_t edges = 0;
  for (const Vertex vertex : component) {
    for (const Vertex next : graph.neighbors[vertex]) {
      if (vertex < next) {
        increment_checked(edges);
      }
    }
  }
  return edges;
}

std::size_t face_count(const SimpleGraph& graph,
                       const std::vector<Vertex>& component,
                       const std::vector<std::vector<Vertex>>& rotation) {
  const std::size_t edges = component_edge_count(graph, component);
  if (edges == 0U) {
    return 1U;
  }

  const std::size_t n = graph.neighbors.size();
  std::vector<std::vector<bool>> used(n, std::vector<bool>(n, false));
  std::size_t faces = 0;

  for (const Vertex from : component) {
    for (const Vertex to : graph.neighbors[from]) {
      if (used[from][to]) {
        continue;
      }
      increment_checked(faces);
      Vertex previous = from;
      Vertex current = to;
      while (!used[previous][current]) {
        used[previous][current] = true;
        const auto& cyclic = rotation[current];
        const auto found = std::find(cyclic.begin(), cyclic.end(), previous);
        if (found == cyclic.end()) {
          throw std::logic_error("rotation system omits an adjacent vertex");
        }
        const std::size_t index =
            static_cast<std::size_t>(found - cyclic.begin());
        const Vertex next = cyclic[(index + 1U) % cyclic.size()];
        previous = current;
        current = next;
      }
    }
  }
  return faces;
}

bool is_genus_zero(const SimpleGraph& graph,
                   const std::vector<Vertex>& component,
                   const std::vector<std::vector<Vertex>>& rotation,
                   std::size_t& faces_out) {
  const std::size_t edges = component_edge_count(graph, component);
  faces_out = face_count(graph, component, rotation);
  const std::size_t vertices = component.size();
  if (vertices > 0U && edges == vertices - 1U) {
    return faces_out == 1U;
  }
  if (edges < vertices) {
    return false;
  }
  const std::size_t difference = edges - vertices;
  if (difference > std::numeric_limits<std::size_t>::max() - 2U) {
    throw std::overflow_error("Euler characteristic arithmetic overflow");
  }
  return faces_out == difference + 2U;
}

struct ComponentSearch {
  const SimpleGraph& graph;
  const std::vector<Vertex>& component;
  std::vector<std::vector<Vertex>>& rotation;
  std::vector<Vertex> variable_vertices;
  std::size_t& tested;
  std::size_t accepted_faces{};

  bool recurse(std::size_t index) {
    if (index == variable_vertices.size()) {
      increment_checked(tested);
      return is_genus_zero(graph, component, rotation, accepted_faces);
    }

    const Vertex vertex = variable_vertices[index];
    const auto& sorted = graph.neighbors[vertex];
    rotation[vertex] = sorted;
    if (sorted.size() <= 2U) {
      return recurse(index + 1U);
    }

    // Cyclic rotations are equivalent, so anchor the smallest neighbor first.
    std::vector<Vertex> suffix(sorted.begin() + 1, sorted.end());
    do {
      rotation[vertex].clear();
      rotation[vertex].push_back(sorted.front());
      rotation[vertex].insert(rotation[vertex].end(), suffix.begin(),
                              suffix.end());
      if (recurse(index + 1U)) {
        return true;
      }
    } while (std::next_permutation(suffix.begin(), suffix.end()));
    return false;
  }
};

}  // namespace

std::optional<PlanarRotationEmbedding> exact_planar_rotation_embedding(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("planarity requires an undirected graph");
  }

  const SimpleGraph simple = simplify(graph);
  PlanarRotationEmbedding result;
  result.clockwise_neighbors = simple.neighbors;

  const auto components = components_of(simple);
  result.component_face_counts.reserve(components.size());

  for (const auto& component : components) {
    const std::size_t vertices = component.size();
    const std::size_t edges = component_edge_count(simple, component);
    if (vertices >= 3U) {
      const std::size_t reduced_vertices = vertices - 2U;
      if (reduced_vertices <=
              std::numeric_limits<std::size_t>::max() / 3U &&
          edges > 3U * reduced_vertices) {
        return std::nullopt;
      }
    }

    std::vector<Vertex> variables = component;
    std::sort(variables.begin(), variables.end(), [&](Vertex first, Vertex second) {
      const auto first_degree = simple.neighbors[first].size();
      const auto second_degree = simple.neighbors[second].size();
      if (first_degree != second_degree) {
        return first_degree > second_degree;
      }
      return first < second;
    });

    ComponentSearch search{simple, component, result.clockwise_neighbors,
                           std::move(variables), result.rotation_systems_tested};
    if (!search.recurse(0U)) {
      return std::nullopt;
    }
    result.component_face_counts.push_back(search.accepted_faces);
  }

  return result;
}

}  // namespace algorithms::graphs
