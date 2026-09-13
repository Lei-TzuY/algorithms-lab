#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace algorithms::data_structures {

class SkipListSet {
 public:
  static constexpr std::size_t kMaxLevel = 64U;

  explicit SkipListSet(std::uint64_t seed = 0x51A17E57ULL)
      : rng_(seed) {
    std::fill(head_.next.begin(), head_.next.end(), nullptr);
  }

  ~SkipListSet() { clear_nodes(); }

  SkipListSet(const SkipListSet&) = delete;
  SkipListSet& operator=(const SkipListSet&) = delete;
  SkipListSet(SkipListSet&&) = delete;
  SkipListSet& operator=(SkipListSet&&) = delete;

  [[nodiscard]] bool insert(std::int64_t key) {
    std::array<Node*, kMaxLevel> update{};
    Node* current = &head_;
    for (std::size_t level = active_levels_; level > 0U; --level) {
      const std::size_t index = level - 1U;
      while (current->next[index] != nullptr &&
             current->next[index]->key < key) {
        current = current->next[index];
      }
      update[index] = current;
    }

    Node* candidate = current->next[0];
    if (candidate != nullptr && candidate->key == key) {
      return false;
    }

    const std::size_t node_levels = random_level_count();
    // Allocate the complete node before mutating structural metadata. If the
    // allocation (including the forward-pointer vector) throws, the represented
    // set and active-level bookkeeping remain unchanged.
    Node* node = new Node(key, node_levels);

    if (node_levels > active_levels_) {
      for (std::size_t level = active_levels_; level < node_levels; ++level) {
        update[level] = &head_;
      }
      active_levels_ = node_levels;
    }

    for (std::size_t level = 0U; level < node_levels; ++level) {
      node->next[level] = update[level]->next[level];
      update[level]->next[level] = node;
    }
    ++size_;
    return true;
  }

  [[nodiscard]] bool erase(std::int64_t key) {
    std::array<Node*, kMaxLevel> update{};
    Node* current = &head_;
    for (std::size_t level = active_levels_; level > 0U; --level) {
      const std::size_t index = level - 1U;
      while (current->next[index] != nullptr &&
             current->next[index]->key < key) {
        current = current->next[index];
      }
      update[index] = current;
    }

    Node* candidate = current->next[0];
    if (candidate == nullptr || candidate->key != key) {
      return false;
    }

    for (std::size_t level = 0U; level < candidate->next.size(); ++level) {
      if (update[level]->next[level] == candidate) {
        update[level]->next[level] = candidate->next[level];
      }
    }
    delete candidate;
    --size_;

    while (active_levels_ > 1U && head_.next[active_levels_ - 1U] == nullptr) {
      --active_levels_;
    }
    return true;
  }

  [[nodiscard]] bool contains(std::int64_t key) const noexcept {
    const Node* current = &head_;
    for (std::size_t level = active_levels_; level > 0U; --level) {
      const std::size_t index = level - 1U;
      while (current->next[index] != nullptr &&
             current->next[index]->key < key) {
        current = current->next[index];
      }
    }
    current = current->next[0];
    return current != nullptr && current->key == key;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t active_level_count() const noexcept {
    return active_levels_;
  }
  [[nodiscard]] std::uint64_t promotion_draw_count() const noexcept {
    return promotion_draw_count_;
  }

  [[nodiscard]] std::vector<std::int64_t> values_in_order() const {
    std::vector<std::int64_t> values;
    values.reserve(size_);
    for (const Node* node = head_.next[0]; node != nullptr; node = node->next[0]) {
      values.push_back(node->key);
    }
    return values;
  }

  [[nodiscard]] std::vector<std::size_t> tower_heights_in_order() const {
    std::vector<std::size_t> heights;
    heights.reserve(size_);
    for (const Node* node = head_.next[0]; node != nullptr; node = node->next[0]) {
      heights.push_back(node->next.size());
    }
    return heights;
  }

  [[nodiscard]] bool valid_structure() const {
    if (active_levels_ == 0U || active_levels_ > kMaxLevel) {
      return false;
    }
    if (size_ == 0U) {
      if (active_levels_ != 1U) {
        return false;
      }
      for (Node* node : head_.next) {
        if (node != nullptr) {
          return false;
        }
      }
      return true;
    }

    for (std::size_t level = active_levels_; level < kMaxLevel; ++level) {
      if (head_.next[level] != nullptr) {
        return false;
      }
    }
    if (head_.next[active_levels_ - 1U] == nullptr) {
      return false;
    }

    std::vector<const Node*> base;
    base.reserve(size_);
    const Node* previous = nullptr;
    for (const Node* node = head_.next[0]; node != nullptr; node = node->next[0]) {
      if (node->next.empty() || node->next.size() > kMaxLevel) {
        return false;
      }
      if (previous != nullptr && previous->key >= node->key) {
        return false;
      }
      base.push_back(node);
      previous = node;
      if (base.size() > size_) {
        return false;
      }
    }
    if (base.size() != size_) {
      return false;
    }

    for (std::size_t level = 1U; level < active_levels_; ++level) {
      const Node* actual = head_.next[level];
      for (const Node* node : base) {
        if (node->next.size() <= level) {
          continue;
        }
        if (actual != node) {
          return false;
        }
        actual = actual->next[level];
      }
      if (actual != nullptr) {
        return false;
      }
    }
    return true;
  }

 private:
  struct Node {
    Node() : key(0), next(kMaxLevel, nullptr) {}
    Node(std::int64_t key_value, std::size_t levels)
        : key(key_value), next(levels, nullptr) {}

    std::int64_t key;
    std::vector<Node*> next;
  };

  [[nodiscard]] std::size_t random_level_count() {
    std::size_t levels = 1U;
    while (levels < kMaxLevel) {
      const std::uint64_t draw = rng_();
      ++promotion_draw_count_;
      if ((draw & UINT64_C(1)) == 0U) {
        break;
      }
      ++levels;
    }
    return levels;
  }

  void clear_nodes() noexcept {
    Node* node = head_.next[0];
    while (node != nullptr) {
      Node* next = node->next[0];
      delete node;
      node = next;
    }
    std::fill(head_.next.begin(), head_.next.end(), nullptr);
    size_ = 0U;
    active_levels_ = 1U;
  }

  Node head_{};
  std::size_t size_ = 0U;
  std::size_t active_levels_ = 1U;
  std::mt19937_64 rng_;
  std::uint64_t promotion_draw_count_ = 0U;
};

}  // namespace algorithms::data_structures
