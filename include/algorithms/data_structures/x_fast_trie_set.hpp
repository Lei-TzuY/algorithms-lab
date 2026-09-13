#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace algorithms::data_structures {

class XFastTrieSet {
 public:
  explicit XFastTrieSet(std::uint8_t universe_bits)
      : universe_bits_(universe_bits),
        levels_(static_cast<std::size_t>(universe_bits) + 1U) {
    if (universe_bits_ == 0U || universe_bits_ > 64U) {
      throw std::invalid_argument("x-fast trie universe width must be in [1,64]");
    }
  }

  [[nodiscard]] std::uint8_t universe_bits() const noexcept {
    return universe_bits_;
  }
  [[nodiscard]] std::size_t size() const noexcept { return leaves_.size(); }
  [[nodiscard]] bool empty() const noexcept { return leaves_.empty(); }

  bool insert(std::uint64_t key) {
    validate_key(key);
    if (leaves_.find(key) != leaves_.end()) {
      return false;
    }

    const auto previous = predecessor_unchecked(key);
    const auto next = successor_unchecked(key);
    leaves_.emplace(key, LeafLinks{previous, next});
    if (previous.has_value()) {
      leaves_.at(*previous).next = key;
    }
    if (next.has_value()) {
      leaves_.at(*next).previous = key;
    }

    for (std::size_t level = 0;
         level <= static_cast<std::size_t>(universe_bits_); ++level) {
      const auto prefix = prefix_of(key, level);
      auto [it, inserted] =
          levels_[level].try_emplace(prefix, PrefixSummary{1U, key, key});
      if (!inserted) {
        ++it->second.count;
        it->second.minimum = std::min(it->second.minimum, key);
        it->second.maximum = std::max(it->second.maximum, key);
      }
    }
    return true;
  }

  bool erase(std::uint64_t key) {
    validate_key(key);
    auto leaf_it = leaves_.find(key);
    if (leaf_it == leaves_.end()) {
      return false;
    }

    const LeafLinks links = leaf_it->second;
    if (links.previous.has_value()) {
      leaves_.at(*links.previous).next = links.next;
    }
    if (links.next.has_value()) {
      leaves_.at(*links.next).previous = links.previous;
    }
    leaves_.erase(leaf_it);

    for (std::size_t reverse = static_cast<std::size_t>(universe_bits_) + 1U;
         reverse > 0U; --reverse) {
      const std::size_t level = reverse - 1U;
      const auto prefix = prefix_of(key, level);
      auto it = levels_[level].find(prefix);
      if (it == levels_[level].end() || it->second.count == 0U) {
        throw std::logic_error("x-fast trie prefix accounting corrupted");
      }
      --it->second.count;
      if (it->second.count == 0U) {
        levels_[level].erase(it);
        continue;
      }
      if (level == static_cast<std::size_t>(universe_bits_)) {
        throw std::logic_error(
            "x-fast trie duplicate leaf accounting corrupted");
      }

      const std::uint64_t left_prefix = prefix << 1U;
      const std::uint64_t right_prefix = left_prefix | 1U;
      const auto left = levels_[level + 1U].find(left_prefix);
      const auto right = levels_[level + 1U].find(right_prefix);
      if (left == levels_[level + 1U].end() &&
          right == levels_[level + 1U].end()) {
        throw std::logic_error("x-fast trie nonempty prefix has no child");
      }

      auto& summary = levels_[level].at(prefix);
      if (left == levels_[level + 1U].end()) {
        summary.minimum = right->second.minimum;
        summary.maximum = right->second.maximum;
      } else if (right == levels_[level + 1U].end()) {
        summary.minimum = left->second.minimum;
        summary.maximum = left->second.maximum;
      } else {
        summary.minimum = left->second.minimum;
        summary.maximum = right->second.maximum;
      }
    }
    return true;
  }

  [[nodiscard]] bool contains(std::uint64_t key) const {
    validate_key(key);
    return leaves_.find(key) != leaves_.end();
  }

  [[nodiscard]] std::optional<std::uint64_t> minimum() const noexcept {
    if (leaves_.empty()) {
      return std::nullopt;
    }
    const auto root = levels_[0].find(0U);
    return root == levels_[0].end()
               ? std::nullopt
               : std::optional<std::uint64_t>{root->second.minimum};
  }

  [[nodiscard]] std::optional<std::uint64_t> maximum() const noexcept {
    if (leaves_.empty()) {
      return std::nullopt;
    }
    const auto root = levels_[0].find(0U);
    return root == levels_[0].end()
               ? std::nullopt
               : std::optional<std::uint64_t>{root->second.maximum};
  }

  [[nodiscard]] std::optional<std::uint64_t> predecessor(
      std::uint64_t key) const {
    validate_key(key);
    return predecessor_unchecked(key);
  }

  [[nodiscard]] std::optional<std::uint64_t> successor(
      std::uint64_t key) const {
    validate_key(key);
    return successor_unchecked(key);
  }

  [[nodiscard]] bool valid_structure() const {
    if (levels_.size() !=
        static_cast<std::size_t>(universe_bits_) + 1U) {
      return false;
    }
    if (leaves_.empty()) {
      for (const auto& level : levels_) {
        if (!level.empty()) {
          return false;
        }
      }
      return true;
    }

    const auto min_key = minimum();
    const auto max_key = maximum();
    if (!min_key.has_value() || !max_key.has_value()) {
      return false;
    }

    std::size_t walked = 0U;
    std::optional<std::uint64_t> previous;
    std::uint64_t current = *min_key;
    while (true) {
      const auto it = leaves_.find(current);
      if (it == leaves_.end() || it->second.previous != previous) {
        return false;
      }
      ++walked;
      if (!it->second.next.has_value()) {
        if (current != *max_key) {
          return false;
        }
        break;
      }
      if (*it->second.next <= current) {
        return false;
      }
      previous = current;
      current = *it->second.next;
      if (walked > leaves_.size()) {
        return false;
      }
    }
    if (walked != leaves_.size()) {
      return false;
    }

    using ExpectedMap =
        std::unordered_map<std::uint64_t, PrefixSummary>;
    std::vector<ExpectedMap> expected(levels_.size());
    for (const auto& [key, ignored_links] : leaves_) {
      static_cast<void>(ignored_links);
      for (std::size_t level = 0; level < levels_.size(); ++level) {
        const auto prefix = prefix_of(key, level);
        auto [it, inserted] = expected[level].try_emplace(
            prefix, PrefixSummary{1U, key, key});
        if (!inserted) {
          ++it->second.count;
          it->second.minimum = std::min(it->second.minimum, key);
          it->second.maximum = std::max(it->second.maximum, key);
        }
      }
    }

    for (std::size_t level = 0; level < levels_.size(); ++level) {
      if (expected[level].size() != levels_[level].size()) {
        return false;
      }
      for (const auto& [prefix, summary] : expected[level]) {
        const auto actual = levels_[level].find(prefix);
        if (actual == levels_[level].end() ||
            actual->second.count != summary.count ||
            actual->second.minimum != summary.minimum ||
            actual->second.maximum != summary.maximum) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  struct PrefixSummary {
    std::size_t count;
    std::uint64_t minimum;
    std::uint64_t maximum;
  };

  struct LeafLinks {
    std::optional<std::uint64_t> previous;
    std::optional<std::uint64_t> next;
  };

  [[nodiscard]] std::uint64_t prefix_of(std::uint64_t key,
                                        std::size_t length) const noexcept {
    if (length == 0U) {
      return 0U;
    }
    const std::size_t width = static_cast<std::size_t>(universe_bits_);
    return key >> (width - length);
  }

  void validate_key(std::uint64_t key) const {
    if (universe_bits_ < 64U) {
      const std::uint64_t limit = std::uint64_t{1} << universe_bits_;
      if (key >= limit) {
        throw std::out_of_range(
            "x-fast trie key outside configured universe");
      }
    }
  }

  [[nodiscard]] std::size_t longest_existing_prefix(
      std::uint64_t key) const {
    if (leaves_.empty()) {
      return 0U;
    }
    std::size_t low = 0U;
    std::size_t high = static_cast<std::size_t>(universe_bits_) + 1U;
    while (low + 1U < high) {
      const std::size_t middle = low + (high - low) / 2U;
      const auto prefix = prefix_of(key, middle);
      if (levels_[middle].find(prefix) != levels_[middle].end()) {
        low = middle;
      } else {
        high = middle;
      }
    }
    return low;
  }

  [[nodiscard]] std::optional<std::uint64_t> predecessor_unchecked(
      std::uint64_t key) const {
    if (leaves_.empty()) {
      return std::nullopt;
    }
    const auto exact = leaves_.find(key);
    if (exact != leaves_.end()) {
      return exact->second.previous;
    }

    const std::size_t level = longest_existing_prefix(key);
    if (level == static_cast<std::size_t>(universe_bits_)) {
      throw std::logic_error(
          "x-fast trie prefix search reached missing exact leaf");
    }
    const auto parent_prefix = prefix_of(key, level);
    const auto parent = levels_[level].find(parent_prefix);
    if (parent == levels_[level].end()) {
      throw std::logic_error("x-fast trie longest prefix missing");
    }

    const std::size_t width = static_cast<std::size_t>(universe_bits_);
    const std::size_t bit_index = width - level - 1U;
    const bool query_bit = ((key >> bit_index) & 1U) != 0U;
    if (query_bit) {
      const auto left =
          levels_[level + 1U].find(parent_prefix << 1U);
      if (left == levels_[level + 1U].end()) {
        throw std::logic_error(
            "x-fast trie missing predecessor sibling");
      }
      return left->second.maximum;
    }

    const auto first_leaf = leaves_.find(parent->second.minimum);
    if (first_leaf == leaves_.end()) {
      throw std::logic_error(
          "x-fast trie parent minimum missing leaf");
    }
    return first_leaf->second.previous;
  }

  [[nodiscard]] std::optional<std::uint64_t> successor_unchecked(
      std::uint64_t key) const {
    if (leaves_.empty()) {
      return std::nullopt;
    }
    const auto exact = leaves_.find(key);
    if (exact != leaves_.end()) {
      return exact->second.next;
    }

    const std::size_t level = longest_existing_prefix(key);
    if (level == static_cast<std::size_t>(universe_bits_)) {
      throw std::logic_error(
          "x-fast trie prefix search reached missing exact leaf");
    }
    const auto parent_prefix = prefix_of(key, level);
    const auto parent = levels_[level].find(parent_prefix);
    if (parent == levels_[level].end()) {
      throw std::logic_error("x-fast trie longest prefix missing");
    }

    const std::size_t width = static_cast<std::size_t>(universe_bits_);
    const std::size_t bit_index = width - level - 1U;
    const bool query_bit = ((key >> bit_index) & 1U) != 0U;
    if (!query_bit) {
      const auto right = levels_[level + 1U].find(
          (parent_prefix << 1U) | 1U);
      if (right == levels_[level + 1U].end()) {
        throw std::logic_error(
            "x-fast trie missing successor sibling");
      }
      return right->second.minimum;
    }

    const auto last_leaf = leaves_.find(parent->second.maximum);
    if (last_leaf == leaves_.end()) {
      throw std::logic_error(
          "x-fast trie parent maximum missing leaf");
    }
    return last_leaf->second.next;
  }

  std::uint8_t universe_bits_;
  std::vector<std::unordered_map<std::uint64_t, PrefixSummary>> levels_;
  std::unordered_map<std::uint64_t, LeafLinks> leaves_;
};

}  // namespace algorithms::data_structures
