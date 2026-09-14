#pragma once

#include "algorithms/data_structures/x_fast_trie_set.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// A bucketed x-fast predecessor set. The representative x-fast trie stores one
// boundary per bucket; each bucket is a first-principles randomized treap.
// Bucket sizes are kept in [w, 4w] (except for the sole bucket) with hysteresis,
// where w is the universe bit width. This removes x-fast's per-key prefix
// replication while retaining expected O(log log U) local/query work.
class YFastTrieSet {
 public:
  explicit YFastTrieSet(std::uint8_t universe_bits, std::uint64_t seed)
      : universe_bits_(universe_bits),
        bucket_target_(std::max<std::size_t>(
            2U, static_cast<std::size_t>(universe_bits))),
        representatives_(universe_bits),
        rng_(seed) {
    if (universe_bits_ == 0U || universe_bits_ > 64U) {
      throw std::invalid_argument("y-fast trie universe width must be in [1,64]");
    }
  }

  YFastTrieSet(const YFastTrieSet&) = delete;
  YFastTrieSet& operator=(const YFastTrieSet&) = delete;
  YFastTrieSet(YFastTrieSet&&) = delete;
  YFastTrieSet& operator=(YFastTrieSet&&) = delete;

  [[nodiscard]] std::uint8_t universe_bits() const noexcept {
    return universe_bits_;
  }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t bucket_target() const noexcept {
    return bucket_target_;
  }
  [[nodiscard]] std::size_t bucket_count() const noexcept {
    return buckets_.size();
  }
  [[nodiscard]] std::size_t representative_count() const noexcept {
    return representatives_.size();
  }

  bool insert(std::uint64_t key) {
    validate_key(key);
    if (empty()) {
      Bucket bucket;
      static_cast<void>(bucket.insert(key, rng_()));
      static_cast<void>(representatives_.insert(0U));
      buckets_.emplace(0U, std::move(bucket));
      size_ = 1U;
      return true;
    }

    const auto separator = separator_for(key);
    auto& bucket = bucket_at(separator);
    if (bucket.contains(key)) {
      return false;
    }
    static_cast<void>(bucket.insert(key, rng_()));
    ++size_;
    if (bucket.size() > maximum_bucket_size()) {
      split_overfull(separator);
    }
    return true;
  }

  bool erase(std::uint64_t key) {
    validate_key(key);
    if (empty()) {
      return false;
    }
    const auto separator = separator_for(key);
    auto& bucket = bucket_at(separator);
    if (!bucket.erase(key)) {
      return false;
    }
    --size_;

    if (size_ == 0U) {
      if (buckets_.size() != 1U || !representatives_.erase(0U)) {
        throw std::logic_error("y-fast trie empty-state accounting corrupted");
      }
      buckets_.clear();
      return true;
    }

    if (buckets_.size() > 1U && bucket_at(separator).size() < bucket_target_) {
      rebalance_underfull(separator);
    }
    return true;
  }

  [[nodiscard]] bool contains(std::uint64_t key) const {
    validate_key(key);
    if (empty()) {
      return false;
    }
    return bucket_at(separator_for(key)).contains(key);
  }

  [[nodiscard]] std::optional<std::uint64_t> minimum() const {
    if (empty()) {
      return std::nullopt;
    }
    return bucket_at(0U).minimum();
  }

  [[nodiscard]] std::optional<std::uint64_t> maximum() const {
    if (empty()) {
      return std::nullopt;
    }
    const auto separator = representatives_.maximum();
    if (!separator.has_value()) {
      throw std::logic_error("y-fast trie missing maximum representative");
    }
    return bucket_at(*separator).maximum();
  }

  [[nodiscard]] std::optional<std::uint64_t> predecessor(
      std::uint64_t key) const {
    validate_key(key);
    if (empty()) {
      return std::nullopt;
    }
    const auto separator = separator_for(key);
    const auto local = bucket_at(separator).predecessor(key);
    if (local.has_value()) {
      return local;
    }
    const auto previous_separator = representatives_.predecessor(separator);
    if (!previous_separator.has_value()) {
      return std::nullopt;
    }
    return bucket_at(*previous_separator).maximum();
  }

  [[nodiscard]] std::optional<std::uint64_t> successor(
      std::uint64_t key) const {
    validate_key(key);
    if (empty()) {
      return std::nullopt;
    }
    const auto separator = separator_for(key);
    const auto local = bucket_at(separator).successor(key);
    if (local.has_value()) {
      return local;
    }
    const auto next_separator = representatives_.successor(separator);
    if (!next_separator.has_value()) {
      return std::nullopt;
    }
    return bucket_at(*next_separator).minimum();
  }

  [[nodiscard]] std::vector<std::uint64_t> values_in_order() const {
    std::vector<std::uint64_t> values;
    values.reserve(size_);
    for (const auto separator : representatives_in_order()) {
      const auto entries = bucket_at(separator).entries_in_order();
      for (const auto& entry : entries) {
        values.push_back(entry.key);
      }
    }
    return values;
  }

  [[nodiscard]] std::vector<std::uint64_t> representatives_in_order() const {
    std::vector<std::uint64_t> representatives;
    if (empty()) {
      return representatives;
    }
    auto current = representatives_.minimum();
    while (current.has_value()) {
      representatives.push_back(*current);
      current = representatives_.successor(*current);
    }
    return representatives;
  }

  [[nodiscard]] std::vector<std::size_t> bucket_sizes_in_order() const {
    std::vector<std::size_t> sizes;
    for (const auto separator : representatives_in_order()) {
      sizes.push_back(bucket_at(separator).size());
    }
    return sizes;
  }

  [[nodiscard]] std::vector<std::uint64_t> bucket_roots_in_order() const {
    std::vector<std::uint64_t> roots;
    for (const auto separator : representatives_in_order()) {
      const auto root = bucket_at(separator).root_key();
      if (!root.has_value()) {
        throw std::logic_error("y-fast trie has empty bucket");
      }
      roots.push_back(*root);
    }
    return roots;
  }

  [[nodiscard]] bool valid_structure() const {
    if (!representatives_.valid_structure()) {
      return false;
    }
    if (empty()) {
      return buckets_.empty() && representatives_.empty();
    }
    if (buckets_.empty() || buckets_.size() != representatives_.size()) {
      return false;
    }

    const auto representatives = representatives_in_order();
    if (representatives.size() != buckets_.size() || representatives.empty() ||
        representatives.front() != 0U) {
      return false;
    }

    std::size_t total = 0U;
    std::optional<std::uint64_t> previous_key;
    for (std::size_t index = 0; index < representatives.size(); ++index) {
      const auto separator = representatives[index];
      const auto found = buckets_.find(separator);
      if (found == buckets_.end() || !found->second.valid_structure() ||
          found->second.empty()) {
        return false;
      }
      const auto bucket_size = found->second.size();
      if (representatives.size() == 1U) {
        if (bucket_size > maximum_bucket_size()) {
          return false;
        }
      } else if (bucket_size < bucket_target_ ||
                 bucket_size > maximum_bucket_size()) {
        return false;
      }

      const auto entries = found->second.entries_in_order();
      const std::optional<std::uint64_t> next_separator =
          index + 1U < representatives.size()
              ? std::optional<std::uint64_t>{representatives[index + 1U]}
              : std::nullopt;
      for (const auto& entry : entries) {
        if (entry.key < separator ||
            (next_separator.has_value() && entry.key >= *next_separator) ||
            (previous_key.has_value() && entry.key <= *previous_key)) {
          return false;
        }
        if (universe_bits_ < 64U &&
            entry.key >= (std::uint64_t{1} << universe_bits_)) {
          return false;
        }
        previous_key = entry.key;
        ++total;
      }
    }
    return total == size_;
  }

 private:
  struct TreapEntry {
    std::uint64_t key;
    std::uint64_t priority;
  };

  class Bucket {
   public:
    Bucket() = default;
    Bucket(Bucket&&) noexcept = default;
    Bucket& operator=(Bucket&&) noexcept = default;
    Bucket(const Bucket&) = delete;
    Bucket& operator=(const Bucket&) = delete;

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }

    bool insert(std::uint64_t key, std::uint64_t priority) {
      if (contains(key)) {
        return false;
      }
      auto node = std::make_unique<Node>(key, priority);
      insert_node(root_, std::move(node));
      ++size_;
      return true;
    }

    bool erase(std::uint64_t key) {
      if (!erase_node(root_, key)) {
        return false;
      }
      --size_;
      return true;
    }

    [[nodiscard]] bool contains(std::uint64_t key) const noexcept {
      const Node* current = root_.get();
      while (current != nullptr) {
        if (key == current->key) {
          return true;
        }
        current = key < current->key ? current->left.get()
                                     : current->right.get();
      }
      return false;
    }

    [[nodiscard]] std::optional<std::uint64_t> minimum() const noexcept {
      const Node* current = root_.get();
      if (current == nullptr) {
        return std::nullopt;
      }
      while (current->left != nullptr) {
        current = current->left.get();
      }
      return current->key;
    }

    [[nodiscard]] std::optional<std::uint64_t> maximum() const noexcept {
      const Node* current = root_.get();
      if (current == nullptr) {
        return std::nullopt;
      }
      while (current->right != nullptr) {
        current = current->right.get();
      }
      return current->key;
    }

    [[nodiscard]] std::optional<std::uint64_t> predecessor(
        std::uint64_t key) const noexcept {
      const Node* current = root_.get();
      std::optional<std::uint64_t> answer;
      while (current != nullptr) {
        if (current->key < key) {
          answer = current->key;
          current = current->right.get();
        } else {
          current = current->left.get();
        }
      }
      return answer;
    }

    [[nodiscard]] std::optional<std::uint64_t> successor(
        std::uint64_t key) const noexcept {
      const Node* current = root_.get();
      std::optional<std::uint64_t> answer;
      while (current != nullptr) {
        if (current->key > key) {
          answer = current->key;
          current = current->left.get();
        } else {
          current = current->right.get();
        }
      }
      return answer;
    }

    [[nodiscard]] std::optional<std::uint64_t> root_key() const noexcept {
      return root_ == nullptr ? std::nullopt
                              : std::optional<std::uint64_t>{root_->key};
    }

    [[nodiscard]] std::vector<TreapEntry> entries_in_order() const {
      std::vector<TreapEntry> entries;
      entries.reserve(size_);
      collect_entries(root_.get(), entries);
      return entries;
    }

    void rebuild(const std::vector<TreapEntry>& entries, std::size_t begin,
                 std::size_t end) {
      root_.reset();
      size_ = 0U;
      for (std::size_t index = begin; index < end; ++index) {
        auto node = std::make_unique<Node>(entries[index].key,
                                           entries[index].priority);
        insert_node(root_, std::move(node));
        ++size_;
      }
    }

    [[nodiscard]] bool valid_structure() const {
      std::size_t counted = 0U;
      if (!validate_node(root_.get(), std::nullopt, std::nullopt, nullptr,
                         counted)) {
        return false;
      }
      return counted == size_;
    }

   private:
    struct Node {
      Node(std::uint64_t key_value, std::uint64_t priority_value)
          : key(key_value), priority(priority_value) {}
      std::uint64_t key;
      std::uint64_t priority;
      std::unique_ptr<Node> left;
      std::unique_ptr<Node> right;
    };

    static bool higher_priority(const Node& first, const Node& second) noexcept {
      return first.priority < second.priority ||
             (first.priority == second.priority && first.key < second.key);
    }

    static void rotate_left(std::unique_ptr<Node>& root) noexcept {
      auto pivot = std::move(root->right);
      root->right = std::move(pivot->left);
      pivot->left = std::move(root);
      root = std::move(pivot);
    }

    static void rotate_right(std::unique_ptr<Node>& root) noexcept {
      auto pivot = std::move(root->left);
      root->left = std::move(pivot->right);
      pivot->right = std::move(root);
      root = std::move(pivot);
    }

    static void insert_node(std::unique_ptr<Node>& root,
                            std::unique_ptr<Node> node) {
      if (root == nullptr) {
        root = std::move(node);
        return;
      }
      const auto key = node->key;
      if (key < root->key) {
        insert_node(root->left, std::move(node));
        if (higher_priority(*root->left, *root)) {
          rotate_right(root);
        }
      } else {
        insert_node(root->right, std::move(node));
        if (higher_priority(*root->right, *root)) {
          rotate_left(root);
        }
      }
    }

    static std::unique_ptr<Node> merge_nodes(std::unique_ptr<Node> left,
                                             std::unique_ptr<Node> right) {
      if (left == nullptr) {
        return right;
      }
      if (right == nullptr) {
        return left;
      }
      if (higher_priority(*left, *right)) {
        left->right = merge_nodes(std::move(left->right), std::move(right));
        return left;
      }
      right->left = merge_nodes(std::move(left), std::move(right->left));
      return right;
    }

    static bool erase_node(std::unique_ptr<Node>& root, std::uint64_t key) {
      if (root == nullptr) {
        return false;
      }
      if (key < root->key) {
        return erase_node(root->left, key);
      }
      if (key > root->key) {
        return erase_node(root->right, key);
      }
      root = merge_nodes(std::move(root->left), std::move(root->right));
      return true;
    }

    static void collect_entries(const Node* node,
                                std::vector<TreapEntry>& entries) {
      if (node == nullptr) {
        return;
      }
      collect_entries(node->left.get(), entries);
      entries.push_back(TreapEntry{node->key, node->priority});
      collect_entries(node->right.get(), entries);
    }

    static bool validate_node(const Node* node,
                              std::optional<std::uint64_t> lower,
                              std::optional<std::uint64_t> upper,
                              const Node* parent, std::size_t& counted) {
      if (node == nullptr) {
        return true;
      }
      if ((lower.has_value() && node->key <= *lower) ||
          (upper.has_value() && node->key >= *upper) ||
          (parent != nullptr && higher_priority(*node, *parent))) {
        return false;
      }
      ++counted;
      return validate_node(node->left.get(), lower, node->key, node, counted) &&
             validate_node(node->right.get(), node->key, upper, node, counted);
    }

    std::unique_ptr<Node> root_;
    std::size_t size_ = 0U;
  };

  [[nodiscard]] std::size_t maximum_bucket_size() const noexcept {
    return 4U * bucket_target_;
  }

  void validate_key(std::uint64_t key) const {
    if (universe_bits_ < 64U) {
      const std::uint64_t limit = std::uint64_t{1} << universe_bits_;
      if (key >= limit) {
        throw std::out_of_range("y-fast trie key outside configured universe");
      }
    }
  }

  [[nodiscard]] std::uint64_t separator_for(std::uint64_t key) const {
    if (representatives_.contains(key)) {
      return key;
    }
    const auto predecessor = representatives_.predecessor(key);
    if (predecessor.has_value()) {
      return *predecessor;
    }
    const auto first = representatives_.minimum();
    if (!first.has_value() || *first != 0U) {
      throw std::logic_error("y-fast trie missing zero bucket boundary");
    }
    return *first;
  }

  Bucket& bucket_at(std::uint64_t separator) {
    auto found = buckets_.find(separator);
    if (found == buckets_.end()) {
      throw std::logic_error("y-fast trie representative has no bucket");
    }
    return found->second;
  }

  const Bucket& bucket_at(std::uint64_t separator) const {
    auto found = buckets_.find(separator);
    if (found == buckets_.end()) {
      throw std::logic_error("y-fast trie representative has no bucket");
    }
    return found->second;
  }

  void split_overfull(std::uint64_t separator) {
    auto entries = bucket_at(separator).entries_in_order();
    if (entries.size() <= maximum_bucket_size()) {
      return;
    }
    const std::size_t middle = entries.size() / 2U;
    if (middle == 0U || middle == entries.size()) {
      throw std::logic_error("y-fast trie cannot split bucket");
    }
    const auto right_separator = entries[middle].key;

    Bucket left;
    left.rebuild(entries, 0U, middle);
    Bucket right;
    right.rebuild(entries, middle, entries.size());
    bucket_at(separator) = std::move(left);
    if (!representatives_.insert(right_separator)) {
      throw std::logic_error("y-fast trie split representative collision");
    }
    auto [ignored, inserted] =
        buckets_.emplace(right_separator, std::move(right));
    static_cast<void>(ignored);
    if (!inserted) {
      throw std::logic_error("y-fast trie split bucket collision");
    }
  }

  void rebalance_underfull(std::uint64_t separator) {
    const auto next = representatives_.successor(separator);
    std::uint64_t left_separator = separator;
    std::uint64_t right_separator = 0U;
    if (next.has_value()) {
      right_separator = *next;
    } else {
      const auto previous = representatives_.predecessor(separator);
      if (!previous.has_value()) {
        throw std::logic_error("y-fast trie underfull bucket has no neighbor");
      }
      left_separator = *previous;
      right_separator = separator;
    }

    auto entries = bucket_at(left_separator).entries_in_order();
    const auto right_entries = bucket_at(right_separator).entries_in_order();
    entries.insert(entries.end(), right_entries.begin(), right_entries.end());

    if (!representatives_.erase(right_separator)) {
      throw std::logic_error("y-fast trie failed to erase merge boundary");
    }
    if (buckets_.erase(right_separator) != 1U) {
      throw std::logic_error("y-fast trie failed to erase merged bucket");
    }

    if (entries.size() <= maximum_bucket_size()) {
      Bucket merged;
      merged.rebuild(entries, 0U, entries.size());
      bucket_at(left_separator) = std::move(merged);
      return;
    }

    const std::size_t middle = entries.size() / 2U;
    const auto new_right_separator = entries[middle].key;
    Bucket left;
    left.rebuild(entries, 0U, middle);
    Bucket right;
    right.rebuild(entries, middle, entries.size());
    bucket_at(left_separator) = std::move(left);
    if (!representatives_.insert(new_right_separator)) {
      throw std::logic_error("y-fast trie rebalance representative collision");
    }
    auto [ignored, inserted] =
        buckets_.emplace(new_right_separator, std::move(right));
    static_cast<void>(ignored);
    if (!inserted) {
      throw std::logic_error("y-fast trie rebalance bucket collision");
    }
  }

  std::uint8_t universe_bits_;
  std::size_t bucket_target_;
  XFastTrieSet representatives_;
  std::unordered_map<std::uint64_t, Bucket> buckets_;
  std::mt19937_64 rng_;
  std::size_t size_ = 0U;
};

}  // namespace algorithms::data_structures
