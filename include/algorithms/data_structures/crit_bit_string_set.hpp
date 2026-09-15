#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class CritBitStringSet {
 public:
  CritBitStringSet() = default;
  CritBitStringSet(const CritBitStringSet&) = delete;
  CritBitStringSet& operator=(const CritBitStringSet&) = delete;
  CritBitStringSet(CritBitStringSet&&) noexcept = default;
  CritBitStringSet& operator=(CritBitStringSet&&) noexcept = default;

  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  [[nodiscard]] bool contains(std::string_view key) const {
    const Node* leaf = find_leaf(key);
    return leaf != nullptr && leaf->key == key;
  }

  bool insert(std::string key) {
    if (!root_) {
      root_ = make_leaf(std::move(key));
      size_ = 1U;
      return true;
    }

    const Node* existing = find_leaf(key);
    if (existing == nullptr) {
      throw std::logic_error("crit-bit tree lost its root-to-leaf route");
    }
    if (existing->key == key) {
      return false;
    }

    const std::size_t differing_bit = first_differing_bit(key, existing->key);
    std::unique_ptr<Node>* slot = &root_;
    while (!(*slot)->leaf && (*slot)->bit_index < differing_bit) {
      Node* node = slot->get();
      slot = bit_at(key, node->bit_index) ? &node->one : &node->zero;
    }

    if (!(*slot)->leaf && (*slot)->bit_index == differing_bit) {
      throw std::logic_error("crit-bit insertion encountered duplicate branch bit");
    }

    auto inserted = make_leaf(std::move(key));
    auto branch = std::make_unique<Node>();
    branch->leaf = false;
    branch->bit_index = differing_bit;
    auto displaced = std::move(*slot);
    if (bit_at(inserted->key, differing_bit)) {
      branch->zero = std::move(displaced);
      branch->one = std::move(inserted);
    } else {
      branch->zero = std::move(inserted);
      branch->one = std::move(displaced);
    }
    *slot = std::move(branch);
    ++size_;
    return true;
  }

  bool erase(std::string_view key) {
    if (!root_) {
      return false;
    }
    if (root_->leaf) {
      if (root_->key != key) {
        return false;
      }
      root_.reset();
      size_ = 0U;
      return true;
    }

    std::unique_ptr<Node>* slot = &root_;
    std::unique_ptr<Node>* parent_slot = nullptr;
    bool selected_one = false;
    while (!(*slot)->leaf) {
      parent_slot = slot;
      Node* node = slot->get();
      selected_one = bit_at(key, node->bit_index);
      slot = selected_one ? &node->one : &node->zero;
    }

    if ((*slot)->key != key) {
      return false;
    }
    if (parent_slot == nullptr) {
      throw std::logic_error("crit-bit erase lost parent for non-root leaf");
    }

    Node* parent = parent_slot->get();
    std::unique_ptr<Node> sibling =
        selected_one ? std::move(parent->zero) : std::move(parent->one);
    if (!sibling) {
      throw std::logic_error("crit-bit internal node missing sibling");
    }
    *parent_slot = std::move(sibling);
    --size_;
    return true;
  }

  [[nodiscard]] std::vector<std::string> values_unsigned_lexicographic() const {
    std::vector<std::string> values;
    values.reserve(size_);
    collect_keys(root_.get(), values);
    std::sort(values.begin(), values.end(), UnsignedLexicographicLess{});
    return values;
  }

  [[nodiscard]] std::size_t internal_node_count() const noexcept {
    return count_internal(root_.get());
  }

  [[nodiscard]] bool valid_structure() const {
    if (!root_) {
      return size_ == 0U;
    }

    std::vector<const Node*> leaves;
    std::size_t internal_count = 0U;
    if (!validate_node(root_.get(), std::nullopt, leaves, internal_count)) {
      return false;
    }
    if (leaves.size() != size_) {
      return false;
    }
    if (internal_count + 1U != leaves.size()) {
      return false;
    }

    std::set<std::string, UnsignedLexicographicLess> unique;
    for (const Node* leaf : leaves) {
      if (!unique.insert(leaf->key).second) {
        return false;
      }
      if (find_leaf(leaf->key) != leaf) {
        return false;
      }
    }
    return validate_canonical_bits(root_.get());
  }

 private:
  struct Node {
    bool leaf{true};
    std::size_t bit_index{};
    std::string key;
    std::unique_ptr<Node> zero;
    std::unique_ptr<Node> one;
  };

  struct UnsignedLexicographicLess {
    bool operator()(std::string_view first, std::string_view second) const {
      const std::size_t common = std::min(first.size(), second.size());
      for (std::size_t index = 0; index < common; ++index) {
        const auto left = static_cast<unsigned char>(first[index]);
        const auto right = static_cast<unsigned char>(second[index]);
        if (left != right) {
          return left < right;
        }
      }
      return first.size() < second.size();
    }
  };

  static std::unique_ptr<Node> make_leaf(std::string key) {
    auto node = std::make_unique<Node>();
    node->leaf = true;
    node->key = std::move(key);
    return node;
  }

  [[nodiscard]] static std::uint16_t symbol_at(std::string_view key,
                                                std::size_t symbol_index) {
    if (symbol_index >= key.size()) {
      return 0U;
    }
    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(key[symbol_index])) + 1U;
  }

  [[nodiscard]] static bool bit_at(std::string_view key, std::size_t bit_index) {
    const std::size_t symbol_index = bit_index / 9U;
    const std::size_t offset = bit_index % 9U;
    const std::uint16_t symbol = symbol_at(key, symbol_index);
    const unsigned shift = static_cast<unsigned>(8U - offset);
    return ((symbol >> shift) & 1U) != 0U;
  }

  [[nodiscard]] static std::size_t first_differing_bit(std::string_view first,
                                                        std::string_view second) {
    const std::size_t limit = std::max(first.size(), second.size());
    for (std::size_t symbol_index = 0; symbol_index <= limit; ++symbol_index) {
      const std::uint16_t left = symbol_at(first, symbol_index);
      const std::uint16_t right = symbol_at(second, symbol_index);
      if (left == right) {
        continue;
      }
      if (symbol_index >
          (std::numeric_limits<std::size_t>::max() - 8U) / 9U) {
        throw std::length_error("crit-bit key bit index is not representable");
      }
      const std::uint16_t difference = static_cast<std::uint16_t>(left ^ right);
      for (std::size_t offset = 0; offset < 9U; ++offset) {
        const unsigned shift = static_cast<unsigned>(8U - offset);
        if (((difference >> shift) & 1U) != 0U) {
          return symbol_index * 9U + offset;
        }
      }
      throw std::logic_error("crit-bit differing symbol has no differing bit");
    }
    throw std::logic_error("crit-bit distinct keys have no differing bit");
  }

  [[nodiscard]] const Node* find_leaf(std::string_view key) const {
    const Node* node = root_.get();
    while (node != nullptr && !node->leaf) {
      node = bit_at(key, node->bit_index) ? node->one.get() : node->zero.get();
    }
    return node;
  }

  static void collect_keys(const Node* node, std::vector<std::string>& output) {
    if (node == nullptr) {
      return;
    }
    if (node->leaf) {
      output.push_back(node->key);
      return;
    }
    collect_keys(node->zero.get(), output);
    collect_keys(node->one.get(), output);
  }

  [[nodiscard]] static std::size_t count_internal(const Node* node) noexcept {
    if (node == nullptr || node->leaf) {
      return 0U;
    }
    return 1U + count_internal(node->zero.get()) + count_internal(node->one.get());
  }

  [[nodiscard]] static bool validate_node(
      const Node* node, std::optional<std::size_t> previous_bit,
      std::vector<const Node*>& leaves, std::size_t& internal_count) {
    if (node == nullptr) {
      return false;
    }
    if (node->leaf) {
      if (node->zero || node->one) {
        return false;
      }
      leaves.push_back(node);
      return true;
    }
    if (!node->zero || !node->one || !node->key.empty()) {
      return false;
    }
    if (previous_bit.has_value() && node->bit_index <= *previous_bit) {
      return false;
    }
    ++internal_count;
    return validate_node(node->zero.get(), node->bit_index, leaves,
                         internal_count) &&
           validate_node(node->one.get(), node->bit_index, leaves,
                         internal_count);
  }

  [[nodiscard]] static const Node* first_leaf(const Node* node) {
    while (node != nullptr && !node->leaf) {
      node = node->zero.get();
    }
    return node;
  }

  [[nodiscard]] static bool validate_canonical_bits(const Node* node) {
    if (node == nullptr || node->leaf) {
      return true;
    }
    const Node* zero_leaf = first_leaf(node->zero.get());
    const Node* one_leaf = first_leaf(node->one.get());
    if (zero_leaf == nullptr || one_leaf == nullptr) {
      return false;
    }
    if (first_differing_bit(zero_leaf->key, one_leaf->key) != node->bit_index) {
      return false;
    }
    if (bit_at(zero_leaf->key, node->bit_index) ||
        !bit_at(one_leaf->key, node->bit_index)) {
      return false;
    }
    return validate_canonical_bits(node->zero.get()) &&
           validate_canonical_bits(node->one.get());
  }

  std::unique_ptr<Node> root_;
  std::size_t size_{};
};

}  // namespace algorithms::data_structures
