#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

// Immutable dominator-tree index for the subgraph reachable from `start`.
//
// Domain / semantics:
// - input must be directed and `start` must be a valid vertex;
// - edge weights are ignored;
// - self-loops and parallel edges are accepted and do not change dominance;
// - unreachable vertices remain outside the dominance domain: they have no DFS
//   number or immediate dominator and no vertex is reported as dominating them.
//
// Construction uses a first-principles Lengauer-Tarjan semi-dominator pass
// with link/eval path compression. This implementation deliberately claims the
// conservative O(VE) worst-case bound of its direct link/eval realization,
// rather than importing a stronger optimized theorem bound. Resident storage is
// O(V+E) over the reachable subgraph plus O(V) public index state.
class DominatorTree {
 public:
  DominatorTree(const Graph& graph, Vertex start);

  [[nodiscard]] std::size_t vertex_count() const noexcept;
  [[nodiscard]] Vertex start() const noexcept;

  [[nodiscard]] bool reachable(Vertex vertex) const;
  [[nodiscard]] std::optional<std::size_t> dfs_number(Vertex vertex) const;
  [[nodiscard]] const std::vector<Vertex>& dfs_preorder() const noexcept;

  // The start vertex and unreachable vertices have no immediate dominator.
  [[nodiscard]] std::optional<Vertex> immediate_dominator(Vertex vertex) const;
  [[nodiscard]] const std::vector<std::optional<Vertex>>&
  immediate_dominators() const noexcept;
  [[nodiscard]] const std::vector<std::vector<Vertex>>& tree_children() const noexcept;

  // Dominance is defined only inside the start-reachable subgraph. Therefore
  // this returns false if either vertex is unreachable. A reachable vertex
  // dominates itself.
  [[nodiscard]] bool dominates(Vertex dominator, Vertex vertex) const;

 private:
  void validate_vertex(Vertex vertex) const;

  std::size_t vertex_count_;
  Vertex start_;
  std::vector<unsigned char> reachable_;
  std::vector<std::size_t> dfs_number_by_vertex_;
  std::vector<Vertex> dfs_preorder_;
  std::vector<std::optional<Vertex>> immediate_dominator_;
  std::vector<std::vector<Vertex>> tree_children_;
  std::vector<std::size_t> tree_entry_;
  std::vector<std::size_t> tree_exit_;
};

}  // namespace algorithms::graphs
