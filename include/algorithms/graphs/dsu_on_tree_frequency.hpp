#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct SubtreeFrequencySummary {
  std::size_t subtree_size{};
  std::size_t distinct_labels{};
  std::size_t max_frequency{};
  std::int64_t mode_label{};

  friend bool operator==(const SubtreeFrequencySummary&,
                         const SubtreeFrequencySummary&) = default;
};

// Immutable rooted-tree frequency index built with the DSU-on-tree ("sack")
// technique. The Graph must be a non-empty undirected simple tree. Edge weights
// are intentionally ignored; labels are snapshotted at construction.
class DsuOnTreeFrequencyIndex {
 public:
  DsuOnTreeFrequencyIndex(const Graph& tree, std::vector<std::int64_t> labels,
                          Vertex root)
      : root_(root), labels_(std::move(labels)) {
    const std::size_t n = tree.vertex_count();
    if (tree.directed()) {
      throw std::invalid_argument("DSU-on-tree index requires an undirected tree");
    }
    if (n == 0) {
      throw std::invalid_argument("DSU-on-tree index requires a non-empty tree");
    }
    if (labels_.size() != n) {
      throw std::invalid_argument("label count must equal tree vertex count");
    }
    tree.validate_vertex(root_);

    parent_.assign(n, std::nullopt);
    children_.assign(n, {});
    subtree_size_.assign(n, 1);
    heavy_child_.assign(n, std::nullopt);
    tin_.assign(n, 0);
    tout_.assign(n, 0);
    build_rooted_tree(tree);
    choose_heavy_children();
    compress_labels();
    summaries_.assign(n, {});
    run_dsu_on_tree();
  }

  [[nodiscard]] std::size_t size() const noexcept { return labels_.size(); }
  [[nodiscard]] Vertex root() const noexcept { return root_; }

  [[nodiscard]] const SubtreeFrequencySummary& summary(Vertex vertex) const {
    validate_vertex(vertex);
    return summaries_[vertex];
  }

  [[nodiscard]] std::optional<Vertex> parent(Vertex vertex) const {
    validate_vertex(vertex);
    return parent_[vertex];
  }

  [[nodiscard]] std::optional<Vertex> heavy_child(Vertex vertex) const {
    validate_vertex(vertex);
    return heavy_child_[vertex];
  }

  [[nodiscard]] std::size_t subtree_size(Vertex vertex) const {
    validate_vertex(vertex);
    return subtree_size_[vertex];
  }

  // Diagnostics for the amortized proof. Every vertex is added once for its
  // retained heavy-chain context and at most once per light ancestor.
  [[nodiscard]] std::size_t add_operations() const noexcept {
    return add_operations_;
  }
  [[nodiscard]] std::size_t remove_operations() const noexcept {
    return remove_operations_;
  }
  [[nodiscard]] std::size_t amortized_add_bound() const noexcept {
    const std::size_t n = size();
    std::size_t floor_log2 = 0;
    for (std::size_t value = n; value > 1; value >>= 1U) {
      ++floor_log2;
    }
    if (n > std::numeric_limits<std::size_t>::max() / (floor_log2 + 1)) {
      return std::numeric_limits<std::size_t>::max();
    }
    return n * (floor_log2 + 1);
  }

 private:
  enum class EventKind {
    kProcess,
    kAddSubtree,
    kAddVertex,
    kAnswer,
    kRemoveSubtree,
  };

  struct Event {
    EventKind kind;
    Vertex vertex;
    bool keep{};
  };

  struct DfsFrame {
    Vertex vertex;
    std::size_t next_neighbor{};
    std::size_t parent_edges_seen{};
  };

  Vertex root_{};
  std::vector<std::int64_t> labels_;
  std::vector<std::optional<Vertex>> parent_;
  std::vector<std::vector<Vertex>> children_;
  std::vector<std::size_t> subtree_size_;
  std::vector<std::optional<Vertex>> heavy_child_;
  std::vector<std::size_t> tin_;
  std::vector<std::size_t> tout_;
  std::vector<Vertex> preorder_;
  std::vector<std::int64_t> unique_labels_;
  std::vector<std::size_t> compressed_label_;
  std::vector<SubtreeFrequencySummary> summaries_;
  std::size_t add_operations_{};
  std::size_t remove_operations_{};

  void validate_vertex(Vertex vertex) const {
    if (vertex >= size()) {
      throw std::out_of_range("DSU-on-tree vertex out of range");
    }
  }

  void build_rooted_tree(const Graph& tree) {
    const std::size_t n = tree.vertex_count();
    std::vector<bool> seen(n, false);
    std::vector<DfsFrame> stack;
    stack.reserve(n);

    seen[root_] = true;
    tin_[root_] = 0;
    preorder_.push_back(root_);
    stack.push_back(DfsFrame{root_, 0, 0});

    while (!stack.empty()) {
      DfsFrame& frame = stack.back();
      const Vertex vertex = frame.vertex;
      const auto& adjacency = tree.neighbors(vertex);
      if (frame.next_neighbor == adjacency.size()) {
        tout_[vertex] = preorder_.size();
        stack.pop_back();
        continue;
      }

      const Vertex next = adjacency[frame.next_neighbor++].to;
      if (next == vertex) {
        throw std::invalid_argument("DSU-on-tree input must not contain self-loops");
      }
      if (parent_[vertex].has_value() && next == *parent_[vertex]) {
        ++frame.parent_edges_seen;
        if (frame.parent_edges_seen > 1) {
          throw std::invalid_argument("DSU-on-tree input must not contain parallel edges");
        }
        continue;
      }
      if (seen[next]) {
        throw std::invalid_argument("DSU-on-tree input must be acyclic and simple");
      }

      seen[next] = true;
      parent_[next] = vertex;
      children_[vertex].push_back(next);
      tin_[next] = preorder_.size();
      preorder_.push_back(next);
      stack.push_back(DfsFrame{next, 0, 0});
    }

    if (preorder_.size() != n) {
      throw std::invalid_argument("DSU-on-tree input must be connected");
    }

    for (auto it = preorder_.rbegin(); it != preorder_.rend(); ++it) {
      const Vertex vertex = *it;
      if (parent_[vertex].has_value()) {
        subtree_size_[*parent_[vertex]] += subtree_size_[vertex];
      }
    }
  }

  void choose_heavy_children() {
    for (const Vertex vertex : preorder_) {
      std::optional<Vertex> best;
      for (const Vertex child : children_[vertex]) {
        if (!best.has_value() || subtree_size_[child] > subtree_size_[*best] ||
            (subtree_size_[child] == subtree_size_[*best] && child < *best)) {
          best = child;
        }
      }
      heavy_child_[vertex] = best;
    }
  }

  void compress_labels() {
    unique_labels_ = labels_;
    std::sort(unique_labels_.begin(), unique_labels_.end());
    unique_labels_.erase(
        std::unique(unique_labels_.begin(), unique_labels_.end()),
        unique_labels_.end());
    compressed_label_.resize(labels_.size());
    for (Vertex vertex = 0; vertex < labels_.size(); ++vertex) {
      const auto it = std::lower_bound(unique_labels_.begin(), unique_labels_.end(),
                                       labels_[vertex]);
      compressed_label_[vertex] =
          static_cast<std::size_t>(it - unique_labels_.begin());
    }
  }

  void run_dsu_on_tree() {
    std::vector<std::size_t> frequency(unique_labels_.size(), 0);
    std::size_t active_count = 0;
    std::size_t distinct_count = 0;
    std::size_t max_frequency = 0;
    std::int64_t mode_label = 0;

    const auto add_vertex = [&](Vertex vertex) {
      const std::size_t label_index = compressed_label_[vertex];
      const std::size_t old_frequency = frequency[label_index];
      ++frequency[label_index];
      ++active_count;
      ++add_operations_;
      if (old_frequency == 0) {
        ++distinct_count;
      }
      const std::size_t new_frequency = old_frequency + 1;
      const std::int64_t label = unique_labels_[label_index];
      if (new_frequency > max_frequency ||
          (new_frequency == max_frequency && label < mode_label)) {
        max_frequency = new_frequency;
        mode_label = label;
      }
    };

    const auto add_subtree = [&](Vertex vertex) {
      for (std::size_t index = tin_[vertex]; index < tout_[vertex]; ++index) {
        add_vertex(preorder_[index]);
      }
    };

    const auto remove_subtree = [&](Vertex vertex) {
      if (active_count != subtree_size_[vertex]) {
        throw std::logic_error("DSU-on-tree removal state is not an isolated subtree");
      }
      for (std::size_t index = tin_[vertex]; index < tout_[vertex]; ++index) {
        const Vertex current = preorder_[index];
        const std::size_t label_index = compressed_label_[current];
        if (frequency[label_index] == 0 || active_count == 0) {
          throw std::logic_error("DSU-on-tree frequency underflow");
        }
        --frequency[label_index];
        --active_count;
        ++remove_operations_;
        if (frequency[label_index] == 0) {
          --distinct_count;
        }
      }
      if (active_count != 0 || distinct_count != 0) {
        throw std::logic_error("DSU-on-tree subtree clear did not restore empty state");
      }
      max_frequency = 0;
      mode_label = 0;
    };

    std::vector<Event> stack;
    stack.push_back(Event{EventKind::kProcess, root_, true});

    while (!stack.empty()) {
      const Event event = stack.back();
      stack.pop_back();
      const Vertex vertex = event.vertex;

      switch (event.kind) {
        case EventKind::kProcess: {
          if (!event.keep) {
            stack.push_back(Event{EventKind::kRemoveSubtree, vertex, false});
          }
          stack.push_back(Event{EventKind::kAnswer, vertex, false});
          stack.push_back(Event{EventKind::kAddVertex, vertex, false});

          const auto heavy = heavy_child_[vertex];
          for (auto it = children_[vertex].rbegin(); it != children_[vertex].rend();
               ++it) {
            if (!heavy.has_value() || *it != *heavy) {
              stack.push_back(Event{EventKind::kAddSubtree, *it, false});
            }
          }
          if (heavy.has_value()) {
            stack.push_back(Event{EventKind::kProcess, *heavy, true});
          }
          for (auto it = children_[vertex].rbegin(); it != children_[vertex].rend();
               ++it) {
            if (!heavy.has_value() || *it != *heavy) {
              stack.push_back(Event{EventKind::kProcess, *it, false});
            }
          }
          break;
        }
        case EventKind::kAddSubtree:
          add_subtree(vertex);
          break;
        case EventKind::kAddVertex:
          add_vertex(vertex);
          break;
        case EventKind::kAnswer:
          if (active_count != subtree_size_[vertex] || max_frequency == 0) {
            throw std::logic_error("DSU-on-tree answer state is not the vertex subtree");
          }
          summaries_[vertex] = SubtreeFrequencySummary{
              subtree_size_[vertex], distinct_count, max_frequency, mode_label};
          break;
        case EventKind::kRemoveSubtree:
          remove_subtree(vertex);
          break;
      }
    }

    if (active_count != size()) {
      throw std::logic_error("DSU-on-tree retained root state has wrong size");
    }
    if (add_operations_ > amortized_add_bound()) {
      throw std::logic_error("DSU-on-tree add-operation bound violated");
    }
  }
};

}  // namespace algorithms::graphs
