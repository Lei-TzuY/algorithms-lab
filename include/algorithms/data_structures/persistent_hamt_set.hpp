#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Persistent immutable Hash Array Mapped Trie set for the full uint64_t domain.
//
// The internal hash mixer is a permutation of uint64_t, so distinct keys have
// distinct 64-bit hashes. Five hash bits are consumed per trie level (the last
// level uses the remaining four bits), giving at most 13 radix levels without
// collision buckets.
//
// insert() and erase() path-copy only the nodes on the modified route and return
// a new value; older set versions remain valid and share untouched subtries.
class PersistentHamtSet64 {
 public:
  using Key = std::uint64_t;

  PersistentHamtSet64() = default;

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }

  [[nodiscard]] bool contains(const Key key) const noexcept {
    return contains_node(root_, key, mix64(key), 0U);
  }

  [[nodiscard]] PersistentHamtSet64 insert(const Key key) const {
    if (contains(key)) {
      return *this;
    }
    if (size_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("persistent HAMT set size exhausted");
    }

    bool inserted = false;
    NodePtr next =
        insert_node(root_, key, mix64(key), 0U, inserted);
    if (!inserted) {
      throw std::logic_error("persistent HAMT insertion made no progress");
    }
    return PersistentHamtSet64(std::move(next), size_ + 1U);
  }

  [[nodiscard]] PersistentHamtSet64 erase(const Key key) const {
    if (!contains(key)) {
      return *this;
    }

    bool erased = false;
    NodePtr next =
        erase_node(root_, key, mix64(key), 0U, erased);
    if (!erased || size_ == 0U) {
      throw std::logic_error("persistent HAMT erase invariant violated");
    }
    return PersistentHamtSet64(std::move(next), size_ - 1U);
  }

  [[nodiscard]] std::size_t reachable_node_count() const noexcept {
    return count_nodes(root_);
  }

  // Expensive structural replay. It is intentionally excluded from operation
  // complexity claims.
  [[nodiscard]] bool valid_structure() const noexcept {
    try {
      std::size_t leaves = 0U;
      if (!validate_node(root_, 0U, leaves)) {
        return false;
      }
      return leaves == size_ && ((root_ == nullptr) == (size_ == 0U));
    } catch (...) {
      return false;
    }
  }

 private:
  static constexpr std::size_t kChunkBits = 5U;
  static constexpr std::size_t kLevels = 13U;

  enum class NodeKind : unsigned char {
    leaf,
    branch,
  };

  struct Node;
  using NodePtr = std::shared_ptr<const Node>;

  struct Node {
    NodeKind kind;
    Key key{};
    std::uint64_t hash{};
    std::uint32_t bitmap{};
    std::vector<NodePtr> children;

    Node(const Key leaf_key, const std::uint64_t leaf_hash)
        : kind(NodeKind::leaf), key(leaf_key), hash(leaf_hash) {}

    Node(const std::uint32_t branch_bitmap,
         std::vector<NodePtr> branch_children)
        : kind(NodeKind::branch),
          bitmap(branch_bitmap),
          children(std::move(branch_children)) {}
  };

  NodePtr root_;
  std::size_t size_{};

  PersistentHamtSet64(NodePtr root, const std::size_t size)
      : root_(std::move(root)), size_(size) {}

  [[nodiscard]] static constexpr std::uint64_t mix64(
      std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31U;
    return value;
  }

  [[nodiscard]] static constexpr std::uint32_t chunk(
      const std::uint64_t hash, const std::size_t depth) noexcept {
    const std::size_t shift = depth * kChunkBits;
    return static_cast<std::uint32_t>((hash >> shift) & UINT64_C(31));
  }

  [[nodiscard]] static constexpr std::uint32_t bit_for(
      const std::uint32_t index) noexcept {
    return std::uint32_t{1} << index;
  }

  [[nodiscard]] static std::size_t child_slot(
      const std::uint32_t bitmap, const std::uint32_t bit) noexcept {
    return static_cast<std::size_t>(
        std::popcount(bitmap & (bit - std::uint32_t{1})));
  }

  [[nodiscard]] static NodePtr make_leaf(
      const Key key, const std::uint64_t hash) {
    return std::make_shared<Node>(key, hash);
  }

  [[nodiscard]] static NodePtr make_branch(
      const std::uint32_t bitmap, std::vector<NodePtr> children) {
    if (bitmap == 0U || children.empty() ||
        static_cast<std::size_t>(std::popcount(bitmap)) != children.size()) {
      throw std::logic_error("persistent HAMT branch shape invalid");
    }
    return std::make_shared<Node>(bitmap, std::move(children));
  }

  [[nodiscard]] static NodePtr normalized_branch(
      const std::uint32_t bitmap, std::vector<NodePtr> children) {
    if (children.empty()) {
      if (bitmap != 0U) {
        throw std::logic_error("persistent HAMT empty branch bitmap mismatch");
      }
      return nullptr;
    }
    if (children.size() == 1U &&
        children.front()->kind == NodeKind::leaf) {
      return children.front();
    }
    return make_branch(bitmap, std::move(children));
  }

  [[nodiscard]] static NodePtr merge_leaves(
      const NodePtr& first, const NodePtr& second,
      const std::size_t depth) {
    if (first == nullptr || second == nullptr ||
        first->kind != NodeKind::leaf ||
        second->kind != NodeKind::leaf ||
        first->key == second->key) {
      throw std::logic_error("persistent HAMT leaf merge invariant violated");
    }
    if (depth >= kLevels) {
      throw std::logic_error(
          "persistent HAMT distinct hashes exhausted all radix bits");
    }

    const std::uint32_t first_index = chunk(first->hash, depth);
    const std::uint32_t second_index = chunk(second->hash, depth);
    const std::uint32_t first_bit = bit_for(first_index);

    if (first_index == second_index) {
      std::vector<NodePtr> children;
      children.push_back(merge_leaves(first, second, depth + 1U));
      return make_branch(first_bit, std::move(children));
    }

    const std::uint32_t second_bit = bit_for(second_index);
    std::vector<NodePtr> children;
    children.reserve(2U);
    if (first_index < second_index) {
      children.push_back(first);
      children.push_back(second);
    } else {
      children.push_back(second);
      children.push_back(first);
    }
    return make_branch(first_bit | second_bit, std::move(children));
  }

  [[nodiscard]] static bool contains_node(
      const NodePtr& node, const Key key, const std::uint64_t hash,
      const std::size_t depth) noexcept {
    if (node == nullptr) {
      return false;
    }
    if (node->kind == NodeKind::leaf) {
      return node->key == key;
    }
    if (depth >= kLevels) {
      return false;
    }

    const std::uint32_t index = chunk(hash, depth);
    const std::uint32_t bit = bit_for(index);
    if ((node->bitmap & bit) == 0U) {
      return false;
    }
    const std::size_t slot = child_slot(node->bitmap, bit);
    return contains_node(node->children[slot], key, hash, depth + 1U);
  }

  [[nodiscard]] static NodePtr insert_node(
      const NodePtr& node, const Key key, const std::uint64_t hash,
      const std::size_t depth, bool& inserted) {
    if (node == nullptr) {
      inserted = true;
      return make_leaf(key, hash);
    }

    if (node->kind == NodeKind::leaf) {
      if (node->key == key) {
        return node;
      }
      inserted = true;
      return merge_leaves(node, make_leaf(key, hash), depth);
    }

    if (depth >= kLevels) {
      throw std::logic_error("persistent HAMT branch exceeds hash width");
    }

    const std::uint32_t index = chunk(hash, depth);
    const std::uint32_t bit = bit_for(index);
    const std::size_t slot = child_slot(node->bitmap, bit);
    std::vector<NodePtr> children = node->children;

    if ((node->bitmap & bit) == 0U) {
      children.insert(children.begin() +
                          static_cast<std::ptrdiff_t>(slot),
                      make_leaf(key, hash));
      inserted = true;
      return make_branch(node->bitmap | bit, std::move(children));
    }

    bool child_inserted = false;
    NodePtr replacement =
        insert_node(children[slot], key, hash, depth + 1U,
                    child_inserted);
    if (!child_inserted) {
      return node;
    }
    children[slot] = std::move(replacement);
    inserted = true;
    return make_branch(node->bitmap, std::move(children));
  }

  [[nodiscard]] static NodePtr erase_node(
      const NodePtr& node, const Key key, const std::uint64_t hash,
      const std::size_t depth, bool& erased) {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->kind == NodeKind::leaf) {
      if (node->key != key) {
        return node;
      }
      erased = true;
      return nullptr;
    }

    if (depth >= kLevels) {
      throw std::logic_error("persistent HAMT branch exceeds hash width");
    }

    const std::uint32_t index = chunk(hash, depth);
    const std::uint32_t bit = bit_for(index);
    if ((node->bitmap & bit) == 0U) {
      return node;
    }

    const std::size_t slot = child_slot(node->bitmap, bit);
    bool child_erased = false;
    NodePtr replacement =
        erase_node(node->children[slot], key, hash, depth + 1U,
                   child_erased);
    if (!child_erased) {
      return node;
    }

    std::vector<NodePtr> children = node->children;
    std::uint32_t bitmap = node->bitmap;
    if (replacement == nullptr) {
      children.erase(children.begin() +
                     static_cast<std::ptrdiff_t>(slot));
      bitmap &= ~bit;
    } else {
      children[slot] = std::move(replacement);
    }

    erased = true;
    return normalized_branch(bitmap, std::move(children));
  }

  [[nodiscard]] static std::size_t count_nodes(
      const NodePtr& node) noexcept {
    if (node == nullptr) {
      return 0U;
    }
    std::size_t total = 1U;
    for (const NodePtr& child : node->children) {
      total += count_nodes(child);
    }
    return total;
  }

  [[nodiscard]] static bool all_match_chunk(
      const NodePtr& node, const std::size_t depth,
      const std::uint32_t expected) noexcept {
    if (node == nullptr) {
      return false;
    }
    if (node->kind == NodeKind::leaf) {
      return chunk(node->hash, depth) == expected;
    }
    for (const NodePtr& child : node->children) {
      if (!all_match_chunk(child, depth, expected)) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static bool validate_node(
      const NodePtr& node, const std::size_t depth,
      std::size_t& leaves) noexcept {
    if (node == nullptr) {
      return true;
    }

    if (node->kind == NodeKind::leaf) {
      if (node->bitmap != 0U || !node->children.empty() ||
          node->hash != mix64(node->key)) {
        return false;
      }
      if (leaves == std::numeric_limits<std::size_t>::max()) {
        return false;
      }
      ++leaves;
      return true;
    }

    if (depth >= kLevels || node->bitmap == 0U ||
        node->children.empty() ||
        static_cast<std::size_t>(std::popcount(node->bitmap)) !=
            node->children.size()) {
      return false;
    }
    for (const NodePtr& child : node->children) {
      if (child == nullptr) {
        return false;
      }
    }
    if (node->children.size() == 1U &&
        node->children.front()->kind == NodeKind::leaf) {
      return false;
    }

    std::size_t slot = 0U;
    for (std::uint32_t index = 0U; index < 32U; ++index) {
      const std::uint32_t bit = bit_for(index);
      if ((node->bitmap & bit) == 0U) {
        continue;
      }
      if (slot >= node->children.size() ||
          !all_match_chunk(node->children[slot], depth, index) ||
          !validate_node(node->children[slot], depth + 1U, leaves)) {
        return false;
      }
      ++slot;
    }
    return slot == node->children.size();
  }
};

}  // namespace algorithms::data_structures
