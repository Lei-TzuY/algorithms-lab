#include "algorithms/graphs/randomized_min_cut.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <queue>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

using UndirectedEdge = std::pair<Vertex, Vertex>;

std::vector<UndirectedEdge> collect_edges(const Graph& graph) {
  std::vector<UndirectedEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        edges.emplace_back(from, edge.to);
      }
    }
  }
  return edges;
}

std::vector<bool> component_from_zero(const Graph& graph) {
  std::vector<bool> seen(graph.vertex_count(), false);
  if (graph.vertex_count() == 0) {
    return seen;
  }

  std::queue<Vertex> queue;
  seen[0] = true;
  queue.push(0);
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (const auto& edge : graph.neighbors(vertex)) {
      if (edge.to == vertex || seen[edge.to]) {
        continue;
      }
      seen[edge.to] = true;
      queue.push(edge.to);
    }
  }
  return seen;
}

bool all_vertices_seen(const std::vector<bool>& seen) {
  return std::all_of(seen.begin(), seen.end(), [](bool value) { return value; });
}

class ContractionState {
 public:
  explicit ContractionState(std::size_t vertex_count)
      : parent_(vertex_count), size_(vertex_count, 1), components_(vertex_count) {
    std::iota(parent_.begin(), parent_.end(), Vertex{0});
  }

  Vertex find(Vertex vertex) {
    while (parent_[vertex] != vertex) {
      parent_[vertex] = parent_[parent_[vertex]];
      vertex = parent_[vertex];
    }
    return vertex;
  }

  bool unite(Vertex first, Vertex second) {
    first = find(first);
    second = find(second);
    if (first == second) {
      return false;
    }
    if (size_[first] < size_[second] ||
        (size_[first] == size_[second] && second < first)) {
      std::swap(first, second);
    }
    parent_[second] = first;
    size_[first] += size_[second];
    --components_;
    return true;
  }

  [[nodiscard]] std::size_t components() const noexcept { return components_; }

 private:
  std::vector<Vertex> parent_;
  std::vector<std::size_t> size_;
  std::size_t components_;
};

std::size_t cut_size_for_side(const std::vector<UndirectedEdge>& edges,
                              const std::vector<bool>& side) {
  std::size_t cut_size = 0;
  for (const auto& [from, to] : edges) {
    if (side[from] != side[to]) {
      ++cut_size;
    }
  }
  return cut_size;
}

std::size_t random_index(std::mt19937_64& rng, std::size_t bound) {
  if (bound == 0) {
    throw std::logic_error("random index requires non-zero bound");
  }
  static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
  const std::uint64_t range = static_cast<std::uint64_t>(bound);
  const std::uint64_t threshold = (std::uint64_t{0} - range) % range;
  while (true) {
    const std::uint64_t value = rng();
    if (value >= threshold) {
      return static_cast<std::size_t>(value % range);
    }
  }
}

struct TrialResult {
  std::size_t cut_size;
  std::vector<bool> side;
};

TrialResult run_trial(std::size_t vertex_count,
                      const std::vector<UndirectedEdge>& edges,
                      std::mt19937_64& rng) {
  ContractionState state(vertex_count);
  std::vector<std::size_t> candidates;
  candidates.reserve(edges.size());

  while (state.components() > 2) {
    candidates.clear();
    for (std::size_t index = 0; index < edges.size(); ++index) {
      const auto& [from, to] = edges[index];
      if (state.find(from) != state.find(to)) {
        candidates.push_back(index);
      }
    }
    if (candidates.empty()) {
      throw std::logic_error("connected graph lost all contraction edges");
    }

    const std::size_t chosen = random_index(rng, candidates.size());
    const auto& [from, to] = edges[candidates[chosen]];
    static_cast<void>(state.unite(from, to));
  }

  std::vector<bool> side(vertex_count, false);
  const Vertex anchor = state.find(0);
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    side[vertex] = state.find(vertex) == anchor;
  }
  return TrialResult{cut_size_for_side(edges, side), std::move(side)};
}

}  // namespace

KargerMinCutResult karger_randomized_global_min_cut(const Graph& graph,
                                         std::uint64_t seed,
                                         std::size_t trials) {
  if (graph.directed()) {
    throw std::invalid_argument("Karger min cut requires undirected input");
  }
  if (trials == 0) {
    throw std::invalid_argument("Karger min cut requires at least one trial");
  }

  const std::size_t vertex_count = graph.vertex_count();
  if (vertex_count <= 1) {
    return KargerMinCutResult{0, std::vector<bool>(vertex_count, false), seed,
                             trials, 0, std::nullopt};
  }

  const auto component = component_from_zero(graph);
  if (!all_vertices_seen(component)) {
    return KargerMinCutResult{0, component, seed, trials, 0, std::nullopt};
  }

  const auto edges = collect_edges(graph);
  std::mt19937_64 rng(seed);

  std::optional<TrialResult> best;
  std::size_t winning_trial = 0;
  for (std::size_t trial = 0; trial < trials; ++trial) {
    TrialResult current = run_trial(vertex_count, edges, rng);
    if (!best.has_value() || current.cut_size < best->cut_size) {
      best = std::move(current);
      winning_trial = trial;
    }
  }

  return KargerMinCutResult{best->cut_size, std::move(best->side), seed, trials,
                           trials, winning_trial};
}

}  // namespace algorithms::graphs
