#include "algorithms/graphs/centroid_nearest_active.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace algorithms::graphs {
namespace {

std::vector<std::vector<Vertex>> validate_and_copy_tree(const Graph& tree) {
  if (tree.directed()) {
    throw std::invalid_argument("centroid decomposition requires an undirected tree");
  }

  const std::size_t n = tree.vertex_count();
  std::vector<std::vector<Vertex>> adjacency(n);
  std::set<std::pair<Vertex, Vertex>> logical_edges;

  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : tree.neighbors(from)) {
      const Vertex to = edge.to;
      if (from == to) {
        throw std::invalid_argument("centroid decomposition rejects self-loops");
      }
      if (from < to) {
        if (!logical_edges.emplace(from, to).second) {
          throw std::invalid_argument("centroid decomposition rejects parallel edges");
        }
        adjacency[from].push_back(to);
        adjacency[to].push_back(from);
      }
    }
  }

  if (n == 0U) {
    return adjacency;
  }
  if (logical_edges.size() != n - 1U) {
    throw std::invalid_argument("centroid decomposition requires exactly V-1 edges");
  }

  std::vector<bool> seen(n, false);
  std::vector<Vertex> stack{0U};
  seen[0] = true;
  std::size_t reached = 0U;
  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    ++reached;
    for (const Vertex next : adjacency[vertex]) {
      if (!seen[next]) {
        seen[next] = true;
        stack.push_back(next);
      }
    }
  }
  if (reached != n) {
    throw std::invalid_argument("centroid decomposition requires a connected tree");
  }

  for (auto& neighbors : adjacency) {
    std::sort(neighbors.begin(), neighbors.end());
  }
  return adjacency;
}

}  // namespace

CentroidNearestActiveIndex::CentroidNearestActiveIndex(const Graph& tree)
    : vertex_count_(tree.vertex_count()),
      adjacency_(validate_and_copy_tree(tree)),
      removed_(vertex_count_, false),
      centroid_parent_(vertex_count_),
      centroid_paths_(vertex_count_),
      active_(vertex_count_, false),
      active_by_centroid_(vertex_count_),
      scratch_parent_(vertex_count_, vertex_count_),
      scratch_subtree_size_(vertex_count_, 0U) {
  if (vertex_count_ != 0U) {
    build_decomposition(0U, std::nullopt);
  }
}

std::size_t CentroidNearestActiveIndex::vertex_count() const noexcept {
  return vertex_count_;
}

bool CentroidNearestActiveIndex::is_active(Vertex vertex) const {
  validate_vertex(vertex);
  return active_[vertex];
}

bool CentroidNearestActiveIndex::activate(Vertex vertex) {
  validate_vertex(vertex);
  if (active_[vertex]) {
    return false;
  }
  for (const CentroidPathEntry& entry : centroid_paths_[vertex]) {
    active_by_centroid_[entry.centroid].emplace(entry.distance, vertex);
  }
  active_[vertex] = true;
  return true;
}

bool CentroidNearestActiveIndex::deactivate(Vertex vertex) {
  validate_vertex(vertex);
  if (!active_[vertex]) {
    return false;
  }
  for (const CentroidPathEntry& entry : centroid_paths_[vertex]) {
    auto& bucket = active_by_centroid_[entry.centroid];
    const auto found = bucket.find({entry.distance, vertex});
    if (found == bucket.end()) {
      throw std::logic_error("centroid active-index invariant broken");
    }
    bucket.erase(found);
  }
  active_[vertex] = false;
  return true;
}

std::optional<NearestActiveVertex> CentroidNearestActiveIndex::nearest_active(
    Vertex vertex) const {
  validate_vertex(vertex);
  std::optional<NearestActiveVertex> best;
  for (const CentroidPathEntry& entry : centroid_paths_[vertex]) {
    const auto& bucket = active_by_centroid_[entry.centroid];
    if (bucket.empty()) {
      continue;
    }
    const auto [active_distance, active_vertex] = *bucket.begin();
    if (entry.distance >
        std::numeric_limits<std::size_t>::max() - active_distance) {
      throw std::overflow_error("centroid distance is not size_t-representable");
    }
    const std::size_t candidate_distance = entry.distance + active_distance;
    const NearestActiveVertex candidate{active_vertex, candidate_distance};
    if (!best.has_value() || candidate.distance < best->distance ||
        (candidate.distance == best->distance &&
         candidate.vertex < best->vertex)) {
      best = candidate;
    }
  }
  return best;
}

const std::vector<std::optional<Vertex>>&
CentroidNearestActiveIndex::centroid_parent() const noexcept {
  return centroid_parent_;
}

void CentroidNearestActiveIndex::validate_vertex(Vertex vertex) const {
  if (vertex >= vertex_count_) {
    throw std::out_of_range("centroid-index vertex out of range");
  }
}

std::vector<Vertex> CentroidNearestActiveIndex::collect_component(
    Vertex start) const {
  struct Pending {
    Vertex vertex;
    Vertex parent;
  };

  std::vector<Vertex> component;
  std::vector<Pending> stack{{start, start}};
  while (!stack.empty()) {
    const Pending current = stack.back();
    stack.pop_back();
    component.push_back(current.vertex);
    for (const Vertex next : adjacency_[current.vertex]) {
      if (!removed_[next] && next != current.parent) {
        stack.push_back(Pending{next, current.vertex});
      }
    }
  }
  return component;
}

Vertex CentroidNearestActiveIndex::choose_centroid(
    const std::vector<Vertex>& component, Vertex root) {
  for (const Vertex vertex : component) {
    scratch_parent_[vertex] = vertex_count_;
    scratch_subtree_size_[vertex] = 0U;
  }

  std::vector<Vertex> order;
  order.reserve(component.size());
  scratch_parent_[root] = root;
  std::vector<Vertex> stack{root};
  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    order.push_back(vertex);
    for (const Vertex next : adjacency_[vertex]) {
      if (removed_[next] || scratch_parent_[next] != vertex_count_) {
        continue;
      }
      scratch_parent_[next] = vertex;
      stack.push_back(next);
    }
  }

  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    const Vertex vertex = *it;
    scratch_subtree_size_[vertex] = 1U;
    for (const Vertex next : adjacency_[vertex]) {
      if (!removed_[next] && scratch_parent_[next] == vertex) {
        scratch_subtree_size_[vertex] += scratch_subtree_size_[next];
      }
    }
  }

  Vertex best = vertex_count_;
  for (const Vertex vertex : component) {
    std::size_t largest_part = component.size() - scratch_subtree_size_[vertex];
    for (const Vertex next : adjacency_[vertex]) {
      if (!removed_[next] && scratch_parent_[next] == vertex) {
        largest_part = std::max(largest_part, scratch_subtree_size_[next]);
      }
    }
    if (largest_part <= component.size() / 2U &&
        (best == vertex_count_ || vertex < best)) {
      best = vertex;
    }
  }
  if (best == vertex_count_) {
    throw std::logic_error("centroid not found in a non-empty tree component");
  }
  return best;
}

void CentroidNearestActiveIndex::append_centroid_distances(Vertex centroid) {
  struct Pending {
    Vertex vertex;
    Vertex parent;
    std::size_t distance;
  };

  std::vector<Pending> stack{{centroid, centroid, 0U}};
  while (!stack.empty()) {
    const Pending current = stack.back();
    stack.pop_back();
    centroid_paths_[current.vertex].push_back(
        CentroidPathEntry{centroid, current.distance});
    for (const Vertex next : adjacency_[current.vertex]) {
      if (removed_[next] || next == current.parent) {
        continue;
      }
      if (current.distance == std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("tree distance is not size_t-representable");
      }
      stack.push_back(Pending{next, current.vertex, current.distance + 1U});
    }
  }
}

void CentroidNearestActiveIndex::build_decomposition(
    Vertex start, std::optional<Vertex> parent) {
  const std::vector<Vertex> component = collect_component(start);
  const Vertex centroid = choose_centroid(component, start);
  centroid_parent_[centroid] = parent;
  append_centroid_distances(centroid);
  removed_[centroid] = true;

  for (const Vertex next : adjacency_[centroid]) {
    if (!removed_[next]) {
      build_decomposition(next, centroid);
    }
  }
}

}  // namespace algorithms::graphs
