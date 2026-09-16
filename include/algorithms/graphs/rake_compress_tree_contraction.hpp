#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

enum class TreeContractionKind { compress, rake };

struct TreeContractionStep {
  TreeContractionKind kind = TreeContractionKind::rake;
  Vertex vertex = 0;
  Vertex first_neighbor = 0;
  std::optional<Vertex> second_neighbor;
  friend bool operator==(const TreeContractionStep&, const TreeContractionStep&) = default;
};

struct TreeContractionRound {
  std::size_t active_before = 0;
  std::vector<TreeContractionStep> compressions;
  std::vector<TreeContractionStep> rakes;
  std::size_t active_after = 0;
  friend bool operator==(const TreeContractionRound&, const TreeContractionRound&) = default;
};

struct RakeCompressTreeContraction {
  Vertex root = 0;
  std::vector<TreeContractionRound> rounds;
  std::vector<std::optional<std::size_t>> removal_round;
  std::vector<std::optional<TreeContractionKind>> removal_kind;
  friend bool operator==(const RakeCompressTreeContraction&, const RakeCompressTreeContraction&) = default;
};

namespace rake_compress_detail {

struct StrictTreeState {
  std::vector<std::set<Vertex>> adjacency;
};

inline StrictTreeState strict_tree_state(const Graph& graph, Vertex root) {
  if (graph.directed()) {
    throw std::invalid_argument("rake-compress contraction requires an undirected tree");
  }
  const std::size_t n = graph.vertex_count();
  if (n == 0) {
    throw std::invalid_argument("rake-compress contraction requires a non-empty tree");
  }
  graph.validate_vertex(root);

  StrictTreeState state;
  state.adjacency.resize(n);
  std::set<std::pair<Vertex, Vertex>> logical_edges;
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      const Vertex to = edge.to;
      if (from == to) {
        throw std::invalid_argument("rake-compress contraction rejects self-loops");
      }
      if (from < to) {
        if (!logical_edges.emplace(from, to).second) {
          throw std::invalid_argument("rake-compress contraction rejects parallel edges");
        }
        state.adjacency[from].insert(to);
        state.adjacency[to].insert(from);
      }
    }
  }
  if (logical_edges.size() != n - 1) {
    throw std::invalid_argument("rake-compress contraction requires exactly n-1 edges");
  }

  std::vector<bool> seen(n, false);
  std::queue<Vertex> pending;
  seen[root] = true;
  pending.push(root);
  std::size_t reached = 0;
  while (!pending.empty()) {
    const Vertex vertex = pending.front();
    pending.pop();
    ++reached;
    for (const Vertex neighbor : state.adjacency[vertex]) {
      if (!seen[neighbor]) {
        seen[neighbor] = true;
        pending.push(neighbor);
      }
    }
  }
  if (reached != n) {
    throw std::invalid_argument("rake-compress contraction requires a connected tree");
  }
  return state;
}

inline std::pair<Vertex, Vertex> two_neighbors(const std::set<Vertex>& neighbors) {
  auto iterator = neighbors.begin();
  const Vertex first = *iterator;
  ++iterator;
  return {first, *iterator};
}

}  // namespace rake_compress_detail

inline RakeCompressTreeContraction rake_compress_tree_contraction(const Graph& graph,
                                                                  Vertex root) {
  auto state = rake_compress_detail::strict_tree_state(graph, root);
  const std::size_t n = graph.vertex_count();

  RakeCompressTreeContraction result;
  result.root = root;
  result.removal_round.resize(n);
  result.removal_kind.resize(n);

  std::vector<bool> active(n, true);
  std::size_t active_count = n;

  while (active_count > 1) {
    TreeContractionRound round;
    round.active_before = active_count;
    const std::size_t round_index = result.rounds.size();

    std::vector<bool> selected(n, false);
    std::vector<Vertex> compression_vertices;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if (!active[vertex] || vertex == root || state.adjacency[vertex].size() != 2) {
        continue;
      }
      bool adjacent_selected = false;
      for (const Vertex neighbor : state.adjacency[vertex]) {
        if (selected[neighbor]) {
          adjacent_selected = true;
          break;
        }
      }
      if (!adjacent_selected) {
        selected[vertex] = true;
        compression_vertices.push_back(vertex);
      }
    }

    for (const Vertex vertex : compression_vertices) {
      if (!active[vertex] || state.adjacency[vertex].size() != 2) {
        throw std::logic_error("rake-compress internal compression invariant failure");
      }
      const auto [first, second] = rake_compress_detail::two_neighbors(state.adjacency[vertex]);
      round.compressions.push_back(
          TreeContractionStep{TreeContractionKind::compress, vertex, first, second});
    }

    for (const Vertex vertex : compression_vertices) {
      const auto [first, second] = rake_compress_detail::two_neighbors(state.adjacency[vertex]);
      state.adjacency[first].erase(vertex);
      state.adjacency[second].erase(vertex);
      state.adjacency[vertex].clear();
      state.adjacency[first].insert(second);
      state.adjacency[second].insert(first);
      active[vertex] = false;
      --active_count;
      result.removal_round[vertex] = round_index;
      result.removal_kind[vertex] = TreeContractionKind::compress;
    }

    std::vector<Vertex> rake_vertices;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if (active[vertex] && vertex != root && state.adjacency[vertex].size() == 1) {
        rake_vertices.push_back(vertex);
      }
    }
    for (const Vertex vertex : rake_vertices) {
      const Vertex neighbor = *state.adjacency[vertex].begin();
      round.rakes.push_back(TreeContractionStep{TreeContractionKind::rake, vertex, neighbor,
                                                std::nullopt});
    }
    for (const Vertex vertex : rake_vertices) {
      const Vertex neighbor = *state.adjacency[vertex].begin();
      state.adjacency[neighbor].erase(vertex);
      state.adjacency[vertex].clear();
      active[vertex] = false;
      --active_count;
      result.removal_round[vertex] = round_index;
      result.removal_kind[vertex] = TreeContractionKind::rake;
    }

    if (round.compressions.empty() && round.rakes.empty()) {
      throw std::logic_error("rake-compress contraction made no progress");
    }
    round.active_after = active_count;
    result.rounds.push_back(std::move(round));
  }

  if (!active[root] || !state.adjacency[root].empty()) {
    throw std::logic_error("rake-compress contraction did not finish at the root");
  }
  return result;
}

inline bool valid_rake_compress_tree_contraction(const Graph& graph,
                                                 const RakeCompressTreeContraction& result) {
  try {
    auto state = rake_compress_detail::strict_tree_state(graph, result.root);
    const std::size_t n = graph.vertex_count();
    if (result.removal_round.size() != n || result.removal_kind.size() != n) {
      return false;
    }
    std::vector<bool> active(n, true);
    std::vector<std::optional<std::size_t>> observed_round(n);
    std::vector<std::optional<TreeContractionKind>> observed_kind(n);
    std::size_t active_count = n;

    for (std::size_t round_index = 0; round_index < result.rounds.size(); ++round_index) {
      const TreeContractionRound& round = result.rounds[round_index];
      if (round.active_before != active_count || active_count <= 1) {
        return false;
      }

      std::vector<bool> selected(n, false);
      Vertex previous = 0;
      bool have_previous = false;
      for (const TreeContractionStep& step : round.compressions) {
        if (step.kind != TreeContractionKind::compress || step.vertex == result.root ||
            !step.second_neighbor || !active[step.vertex] ||
            state.adjacency[step.vertex].size() != 2) {
          return false;
        }
        if (have_previous && step.vertex <= previous) {
          return false;
        }
        previous = step.vertex;
        have_previous = true;
        const auto [first, second] = rake_compress_detail::two_neighbors(state.adjacency[step.vertex]);
        if (step.first_neighbor != first || *step.second_neighbor != second) {
          return false;
        }
        for (const Vertex neighbor : state.adjacency[step.vertex]) {
          if (selected[neighbor]) {
            return false;
          }
        }
        selected[step.vertex] = true;
      }

      for (Vertex vertex = 0; vertex < n; ++vertex) {
        if (!active[vertex] || vertex == result.root || state.adjacency[vertex].size() != 2 ||
            selected[vertex]) {
          continue;
        }
        bool has_selected_neighbor = false;
        for (const Vertex neighbor : state.adjacency[vertex]) {
          if (selected[neighbor]) {
            has_selected_neighbor = true;
            break;
          }
        }
        if (!has_selected_neighbor) {
          return false;
        }
      }

      for (const TreeContractionStep& step : round.compressions) {
        const Vertex vertex = step.vertex;
        const auto [first, second] = rake_compress_detail::two_neighbors(state.adjacency[vertex]);
        state.adjacency[first].erase(vertex);
        state.adjacency[second].erase(vertex);
        state.adjacency[vertex].clear();
        state.adjacency[first].insert(second);
        state.adjacency[second].insert(first);
        active[vertex] = false;
        --active_count;
        observed_round[vertex] = round_index;
        observed_kind[vertex] = TreeContractionKind::compress;
      }

      std::vector<Vertex> expected_rakes;
      for (Vertex vertex = 0; vertex < n; ++vertex) {
        if (active[vertex] && vertex != result.root && state.adjacency[vertex].size() == 1) {
          expected_rakes.push_back(vertex);
        }
      }
      if (round.rakes.size() != expected_rakes.size()) {
        return false;
      }
      for (std::size_t index = 0; index < expected_rakes.size(); ++index) {
        const TreeContractionStep& step = round.rakes[index];
        const Vertex vertex = expected_rakes[index];
        if (step.kind != TreeContractionKind::rake || step.vertex != vertex || step.second_neighbor ||
            !active[vertex] || state.adjacency[vertex].size() != 1 ||
            step.first_neighbor != *state.adjacency[vertex].begin()) {
          return false;
        }
      }
      for (const Vertex vertex : expected_rakes) {
        const Vertex neighbor = *state.adjacency[vertex].begin();
        state.adjacency[neighbor].erase(vertex);
        state.adjacency[vertex].clear();
        active[vertex] = false;
        --active_count;
        observed_round[vertex] = round_index;
        observed_kind[vertex] = TreeContractionKind::rake;
      }

      if (round.active_after != active_count) {
        return false;
      }
    }

    if (active_count != 1 || !active[result.root] || !state.adjacency[result.root].empty()) {
      return false;
    }
    if (observed_round != result.removal_round || observed_kind != result.removal_kind) {
      return false;
    }
    if (result.removal_round[result.root] || result.removal_kind[result.root]) {
      return false;
    }
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace algorithms::graphs
