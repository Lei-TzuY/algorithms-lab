#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class EulerTourForest {
 public:
  explicit EulerTourForest(std::size_t vertex_count,
                           std::uint64_t seed = 0xE17E5EEDULL)
      : rng_(seed), vertices_(vertex_count) {
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
      vertices_[vertex] = make_node(true, vertex, vertex);
    }
  }

  EulerTourForest(const EulerTourForest&) = delete;
  EulerTourForest& operator=(const EulerTourForest&) = delete;
  EulerTourForest(EulerTourForest&&) noexcept = default;
  EulerTourForest& operator=(EulerTourForest&&) noexcept = default;

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return vertices_.size();
  }

  [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }

  [[nodiscard]] bool connected(std::size_t first, std::size_t second) const {
    validate_vertex(first);
    validate_vertex(second);
    return root_of(vertices_[first].get()) == root_of(vertices_[second].get());
  }

  [[nodiscard]] std::size_t component_size(std::size_t vertex) const {
    validate_vertex(vertex);
    return vertex_count_of(root_of(vertices_[vertex].get()));
  }

  void link(std::size_t first, std::size_t second) {
    validate_vertex(first);
    validate_vertex(second);
    if (first == second) {
      throw std::invalid_argument("EulerTourForest cannot link a vertex to itself");
    }
    if (connected(first, second)) {
      throw std::invalid_argument("EulerTourForest link would create a cycle");
    }

    const EdgeKey key = canonical_key(first, second);
    if (edges_.contains(key)) {
      throw std::logic_error("EulerTourForest edge bookkeeping is inconsistent");
    }

    EdgeRecord record;
    record.first_to_second = make_node(false, key.first, key.second);
    record.second_to_first = make_node(false, key.second, key.first);
    Node* const first_to_second =
        first == key.first ? record.first_to_second.get()
                           : record.second_to_first.get();
    Node* const second_to_first =
        first == key.first ? record.second_to_first.get()
                           : record.first_to_second.get();

    auto [it, inserted] = edges_.emplace(key, std::move(record));
    if (!inserted) {
      throw std::logic_error("EulerTourForest duplicate edge bookkeeping");
    }

    Node* first_root = rotate_to_front(vertices_[first].get());
    Node* second_root = rotate_to_front(vertices_[second].get());

    Node* merged = merge(first_root, first_to_second);
    merged = merge(merged, second_root);
    static_cast<void>(merge(merged, second_to_first));
    static_cast<void>(it);
  }

  void cut(std::size_t first, std::size_t second) {
    validate_vertex(first);
    validate_vertex(second);
    if (first == second) {
      throw std::invalid_argument("EulerTourForest self-edge does not exist");
    }

    const EdgeKey key = canonical_key(first, second);
    auto it = edges_.find(key);
    if (it == edges_.end()) {
      throw std::invalid_argument("EulerTourForest cut requires an existing edge");
    }

    Node* forward = nullptr;
    Node* backward = nullptr;
    if (key.first == first) {
      forward = it->second.first_to_second.get();
      backward = it->second.second_to_first.get();
    } else {
      forward = it->second.second_to_first.get();
      backward = it->second.first_to_second.get();
    }

    Node* root = rotate_to_front(forward);
    auto [forward_only, rest] = split(root, 1);
    if (forward_only != forward) {
      throw std::logic_error("EulerTourForest forward edge isolation failed");
    }

    const std::size_t backward_rank = rank_of(backward);
    auto [middle, backward_and_tail] = split(rest, backward_rank);
    auto [backward_only, tail] = split(backward_and_tail, 1);
    if (backward_only != backward || middle == nullptr || tail == nullptr) {
      throw std::logic_error("EulerTourForest cut decomposition failed");
    }

    // Both component roots are intentionally retained only through their member
    // nodes. The detached edge tokens can now be destroyed safely.
    static_cast<void>(middle);
    static_cast<void>(tail);
    detach_single(forward_only);
    detach_single(backward_only);
    edges_.erase(it);
  }

  [[nodiscard]] bool valid_structure() const {
    if (edges_.size() > vertices_.size()) {
      return false;
    }
    if (edges_.size() >
        (std::numeric_limits<std::size_t>::max() - vertices_.size()) / 2) {
      return false;
    }
    const std::size_t expected_nodes = vertices_.size() + 2 * edges_.size();
    std::size_t seen_nodes = 0;
    std::size_t roots = 0;
    std::map<const Node*, bool> seen;

    auto inspect_root = [&](const Node* root) -> bool {
      if (root == nullptr || root->parent != nullptr) {
        return false;
      }
      struct Frame {
        const Node* node;
        bool expanded;
      };
      std::vector<Frame> stack;
      stack.push_back(Frame{root, false});
      while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();
        const Node* node = frame.node;
        if (node == nullptr) {
          continue;
        }
        if (!frame.expanded) {
          if (seen.contains(node)) {
            return false;
          }
          seen.emplace(node, true);
          ++seen_nodes;
          stack.push_back(Frame{node, true});
          if (node->right != nullptr) {
            if (node->right->parent != node || higher_priority(node->right, node)) {
              return false;
            }
            stack.push_back(Frame{node->right, false});
          }
          if (node->left != nullptr) {
            if (node->left->parent != node || higher_priority(node->left, node)) {
              return false;
            }
            stack.push_back(Frame{node->left, false});
          }
        } else {
          const std::size_t exact_size =
              1 + size_of(node->left) + size_of(node->right);
          const std::size_t exact_vertices =
              (node->is_vertex ? 1U : 0U) + vertex_count_of(node->left) +
              vertex_count_of(node->right);
          if (node->size != exact_size || node->vertex_count != exact_vertices) {
            return false;
          }
        }
      }
      return true;
    };

    std::map<const Node*, bool> root_seen;
    for (const auto& vertex : vertices_) {
      const Node* root = root_of(vertex.get());
      if (!root_seen.contains(root)) {
        root_seen.emplace(root, true);
        ++roots;
        if (!inspect_root(root)) {
          return false;
        }
      }
      if (!seen.contains(vertex.get()) || !vertex->is_vertex) {
        return false;
      }
    }

    for (const auto& [key, record] : edges_) {
      const Node* first = record.first_to_second.get();
      const Node* second = record.second_to_first.get();
      if (first == nullptr || second == nullptr || first->is_vertex || second->is_vertex) {
        return false;
      }
      if (first->from != key.first || first->to != key.second ||
          second->from != key.second || second->to != key.first) {
        return false;
      }
      if (!seen.contains(first) || !seen.contains(second) ||
          root_of(first) != root_of(second)) {
        return false;
      }
    }

    if (seen_nodes != expected_nodes || seen.size() != expected_nodes) {
      return false;
    }
    return roots + edges_.size() == vertices_.size();
  }

 private:
  struct Node {
    Node* left = nullptr;
    Node* right = nullptr;
    Node* parent = nullptr;
    std::uint64_t priority = 0;
    std::uint64_t serial = 0;
    std::size_t size = 1;
    std::size_t vertex_count = 0;
    bool is_vertex = false;
    std::size_t from = 0;
    std::size_t to = 0;
  };

  struct EdgeKey {
    std::size_t first;
    std::size_t second;
    friend bool operator<(const EdgeKey& lhs, const EdgeKey& rhs) noexcept {
      return lhs.first < rhs.first ||
             (lhs.first == rhs.first && lhs.second < rhs.second);
    }
  };

  struct EdgeRecord {
    std::unique_ptr<Node> first_to_second;
    std::unique_ptr<Node> second_to_first;
  };

  [[nodiscard]] std::unique_ptr<Node> make_node(bool is_vertex,
                                                std::size_t from,
                                                std::size_t to) {
    if (next_serial_ == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error("EulerTourForest token serial exhausted");
    }
    auto node = std::make_unique<Node>();
    node->priority = rng_();
    node->serial = next_serial_++;
    node->vertex_count = is_vertex ? 1U : 0U;
    node->is_vertex = is_vertex;
    node->from = from;
    node->to = to;
    return node;
  }

  static EdgeKey canonical_key(std::size_t first, std::size_t second) noexcept {
    return first < second ? EdgeKey{first, second} : EdgeKey{second, first};
  }

  void validate_vertex(std::size_t vertex) const {
    if (vertex >= vertices_.size()) {
      throw std::out_of_range("EulerTourForest vertex out of range");
    }
  }

  static std::size_t size_of(const Node* node) noexcept {
    return node == nullptr ? 0U : node->size;
  }

  static std::size_t vertex_count_of(const Node* node) noexcept {
    return node == nullptr ? 0U : node->vertex_count;
  }

  static bool higher_priority(const Node* first, const Node* second) noexcept {
    if (first->priority != second->priority) {
      return first->priority > second->priority;
    }
    return first->serial > second->serial;
  }

  static void pull(Node* node) noexcept {
    node->size = 1 + size_of(node->left) + size_of(node->right);
    node->vertex_count = (node->is_vertex ? 1U : 0U) +
                         vertex_count_of(node->left) +
                         vertex_count_of(node->right);
    if (node->left != nullptr) {
      node->left->parent = node;
    }
    if (node->right != nullptr) {
      node->right->parent = node;
    }
  }

  static Node* merge(Node* first, Node* second) noexcept {
    if (first == nullptr) {
      if (second != nullptr) {
        second->parent = nullptr;
      }
      return second;
    }
    if (second == nullptr) {
      first->parent = nullptr;
      return first;
    }

    if (higher_priority(first, second)) {
      first->right = merge(first->right, second);
      pull(first);
      first->parent = nullptr;
      return first;
    }
    second->left = merge(first, second->left);
    pull(second);
    second->parent = nullptr;
    return second;
  }

  static std::pair<Node*, Node*> split(Node* root, std::size_t prefix_size) noexcept {
    if (root == nullptr) {
      return {nullptr, nullptr};
    }

    const std::size_t left_size = size_of(root->left);
    if (prefix_size <= left_size) {
      auto [first, second] = split(root->left, prefix_size);
      root->left = second;
      pull(root);
      root->parent = nullptr;
      if (first != nullptr) {
        first->parent = nullptr;
      }
      return {first, root};
    }

    auto [first, second] =
        split(root->right, prefix_size - left_size - 1);
    root->right = first;
    pull(root);
    root->parent = nullptr;
    if (second != nullptr) {
      second->parent = nullptr;
    }
    return {root, second};
  }

  static Node* root_of(Node* node) noexcept {
    while (node->parent != nullptr) {
      node = node->parent;
    }
    return node;
  }

  static const Node* root_of(const Node* node) noexcept {
    while (node->parent != nullptr) {
      node = node->parent;
    }
    return node;
  }

  static std::size_t rank_of(const Node* node) noexcept {
    std::size_t rank = size_of(node->left);
    while (node->parent != nullptr) {
      const Node* parent = node->parent;
      if (node == parent->right) {
        rank += 1 + size_of(parent->left);
      }
      node = parent;
    }
    return rank;
  }

  static Node* rotate_to_front(Node* node) noexcept {
    Node* root = root_of(node);
    const std::size_t rank = rank_of(node);
    auto [before, from_node] = split(root, rank);
    return merge(from_node, before);
  }

  static void detach_single(Node* node) noexcept {
    if (node == nullptr) {
      return;
    }
    node->left = nullptr;
    node->right = nullptr;
    node->parent = nullptr;
    node->size = 1;
    node->vertex_count = node->is_vertex ? 1U : 0U;
  }

  std::mt19937_64 rng_;
  std::uint64_t next_serial_ = 0;
  std::vector<std::unique_ptr<Node>> vertices_;
  std::map<EdgeKey, EdgeRecord> edges_;
};

}  // namespace algorithms::data_structures
