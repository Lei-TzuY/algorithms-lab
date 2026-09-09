#include "algorithms/graphs/dominator_tree.hpp"

#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {
namespace {

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

std::size_t evaluate_link_path(std::size_t vertex,
                               std::vector<std::size_t>& ancestor,
                               std::vector<std::size_t>& label,
                               const std::vector<std::size_t>& semi) {
  if (ancestor[vertex] == kNoIndex) {
    return label[vertex];
  }

  std::vector<std::size_t> path;
  std::size_t current = vertex;
  while (ancestor[current] != kNoIndex &&
         ancestor[ancestor[current]] != kNoIndex) {
    path.push_back(current);
    current = ancestor[current];
  }

  for (auto iterator = path.rbegin(); iterator != path.rend(); ++iterator) {
    const std::size_t node = *iterator;
    const std::size_t parent = ancestor[node];
    if (semi[label[parent]] < semi[label[node]]) {
      label[node] = label[parent];
    }
    ancestor[node] = ancestor[parent];
  }
  return label[vertex];
}

}  // namespace

DominatorTree::DominatorTree(const Graph& graph, Vertex start)
    : vertex_count_(graph.vertex_count()),
      start_(start),
      reachable_(vertex_count_, 0U),
      dfs_number_by_vertex_(vertex_count_, kNoIndex),
      immediate_dominator_(vertex_count_),
      tree_children_(vertex_count_),
      tree_entry_(vertex_count_, kNoIndex),
      tree_exit_(vertex_count_, kNoIndex) {
  graph.validate_vertex(start_);
  if (!graph.directed()) {
    throw std::invalid_argument("dominator tree requires a directed graph");
  }

  struct DfsFrame {
    Vertex vertex;
    std::size_t next_neighbor;
  };

  std::vector<std::size_t> parent_by_dfs;
  std::vector<DfsFrame> stack;
  dfs_number_by_vertex_[start_] = 0U;
  reachable_[start_] = 1U;
  dfs_preorder_.push_back(start_);
  parent_by_dfs.push_back(kNoIndex);
  stack.push_back(DfsFrame{start_, 0U});

  while (!stack.empty()) {
    DfsFrame& frame = stack.back();
    const auto& neighbors = graph.neighbors(frame.vertex);
    if (frame.next_neighbor == neighbors.size()) {
      stack.pop_back();
      continue;
    }

    const Vertex target = neighbors[frame.next_neighbor].to;
    ++frame.next_neighbor;
    if (dfs_number_by_vertex_[target] != kNoIndex) {
      continue;
    }

    const std::size_t parent_index = dfs_number_by_vertex_[frame.vertex];
    const std::size_t target_index = dfs_preorder_.size();
    dfs_number_by_vertex_[target] = target_index;
    reachable_[target] = 1U;
    dfs_preorder_.push_back(target);
    parent_by_dfs.push_back(parent_index);
    stack.push_back(DfsFrame{target, 0U});
  }

  const std::size_t reachable_count = dfs_preorder_.size();
  std::vector<std::vector<std::size_t>> predecessors(reachable_count);
  for (std::size_t source_index = 0U; source_index < reachable_count;
       ++source_index) {
    const Vertex source = dfs_preorder_[source_index];
    for (const Edge& edge : graph.neighbors(source)) {
      const std::size_t target_index = dfs_number_by_vertex_[edge.to];
      if (target_index != kNoIndex) {
        predecessors[target_index].push_back(source_index);
      }
    }
  }

  std::vector<std::size_t> semi(reachable_count);
  std::vector<std::size_t> label(reachable_count);
  std::vector<std::size_t> ancestor(reachable_count, kNoIndex);
  std::vector<std::size_t> immediate_by_dfs(reachable_count, kNoIndex);
  immediate_by_dfs[0] = 0U;
  std::vector<std::vector<std::size_t>> bucket(reachable_count);
  for (std::size_t index = 0U; index < reachable_count; ++index) {
    semi[index] = index;
    label[index] = index;
  }

  for (std::size_t cursor = reachable_count; cursor > 1U; --cursor) {
    const std::size_t vertex = cursor - 1U;
    for (const std::size_t predecessor : predecessors[vertex]) {
      const std::size_t candidate =
          evaluate_link_path(predecessor, ancestor, label, semi);
      if (semi[candidate] < semi[vertex]) {
        semi[vertex] = semi[candidate];
      }
    }

    bucket[semi[vertex]].push_back(vertex);
    const std::size_t parent = parent_by_dfs[vertex];
    ancestor[vertex] = parent;

    for (const std::size_t pending : bucket[parent]) {
      const std::size_t candidate =
          evaluate_link_path(pending, ancestor, label, semi);
      immediate_by_dfs[pending] =
          (semi[candidate] < semi[pending]) ? candidate : parent;
    }
    bucket[parent].clear();
  }

  for (std::size_t vertex = 1U; vertex < reachable_count; ++vertex) {
    if (immediate_by_dfs[vertex] == kNoIndex) {
      throw std::logic_error("dominator construction left an idom unresolved");
    }
    if (immediate_by_dfs[vertex] != semi[vertex]) {
      immediate_by_dfs[vertex] = immediate_by_dfs[immediate_by_dfs[vertex]];
    }
  }

  for (std::size_t index = 1U; index < reachable_count; ++index) {
    const Vertex vertex = dfs_preorder_[index];
    const Vertex parent = dfs_preorder_[immediate_by_dfs[index]];
    immediate_dominator_[vertex] = parent;
    tree_children_[parent].push_back(vertex);
  }

  struct TreeFrame {
    Vertex vertex;
    std::size_t next_child;
  };

  std::size_t timer = 0U;
  std::vector<TreeFrame> tree_stack;
  tree_entry_[start_] = timer;
  ++timer;
  tree_stack.push_back(TreeFrame{start_, 0U});
  while (!tree_stack.empty()) {
    TreeFrame& frame = tree_stack.back();
    const auto& children = tree_children_[frame.vertex];
    if (frame.next_child == children.size()) {
      tree_exit_[frame.vertex] = timer;
      tree_stack.pop_back();
      continue;
    }

    const Vertex child = children[frame.next_child];
    ++frame.next_child;
    tree_entry_[child] = timer;
    ++timer;
    tree_stack.push_back(TreeFrame{child, 0U});
  }
}

std::size_t DominatorTree::vertex_count() const noexcept { return vertex_count_; }

Vertex DominatorTree::start() const noexcept { return start_; }

bool DominatorTree::reachable(Vertex vertex) const {
  validate_vertex(vertex);
  return reachable_[vertex] != 0U;
}

std::optional<std::size_t> DominatorTree::dfs_number(Vertex vertex) const {
  validate_vertex(vertex);
  if (dfs_number_by_vertex_[vertex] == kNoIndex) {
    return std::nullopt;
  }
  return dfs_number_by_vertex_[vertex];
}

const std::vector<Vertex>& DominatorTree::dfs_preorder() const noexcept {
  return dfs_preorder_;
}

std::optional<Vertex> DominatorTree::immediate_dominator(Vertex vertex) const {
  validate_vertex(vertex);
  return immediate_dominator_[vertex];
}

const std::vector<std::optional<Vertex>>&
DominatorTree::immediate_dominators() const noexcept {
  return immediate_dominator_;
}

const std::vector<std::vector<Vertex>>& DominatorTree::tree_children() const noexcept {
  return tree_children_;
}

bool DominatorTree::dominates(Vertex dominator, Vertex vertex) const {
  validate_vertex(dominator);
  validate_vertex(vertex);
  if (reachable_[dominator] == 0U || reachable_[vertex] == 0U) {
    return false;
  }
  return tree_entry_[dominator] <= tree_entry_[vertex] &&
         tree_exit_[vertex] <= tree_exit_[dominator];
}

void DominatorTree::validate_vertex(Vertex vertex) const {
  if (vertex >= vertex_count_) {
    throw std::out_of_range("dominator-tree vertex out of range");
  }
}

}  // namespace algorithms::graphs
