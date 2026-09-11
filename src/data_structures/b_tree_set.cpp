#include "algorithms/data_structures/b_tree_set.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace algorithms::data_structures {

struct BTreeSet::Node {
  explicit Node(bool is_leaf) : leaf(is_leaf) {}

  bool leaf;
  std::vector<std::int64_t> keys;
  std::vector<std::unique_ptr<Node>> children;
};

BTreeSet::BTreeSet(std::size_t minimum_degree)
    : minimum_degree_(minimum_degree), root_(std::make_unique<Node>(true)) {
  if (minimum_degree_ < 2U) {
    throw std::invalid_argument("B-tree minimum degree must be at least two");
  }
  if (minimum_degree_ >
      (std::numeric_limits<std::size_t>::max() - 1U) / 2U) {
    throw std::length_error("B-tree minimum degree is too large");
  }
}

BTreeSet::~BTreeSet() = default;

std::size_t BTreeSet::minimum_degree() const noexcept {
  return minimum_degree_;
}

std::size_t BTreeSet::size() const noexcept { return size_; }

bool BTreeSet::empty() const noexcept { return size_ == 0U; }

std::size_t BTreeSet::height() const noexcept {
  std::size_t result = 0U;
  const Node* node = root_.get();
  while (node != nullptr && !node->leaf) {
    ++result;
    node = node->children.front().get();
  }
  return result;
}

std::size_t BTreeSet::max_keys() const noexcept {
  return minimum_degree_ * 2U - 1U;
}

std::size_t BTreeSet::lower_bound_index(const Node& node,
                                        std::int64_t key) const noexcept {
  std::size_t low = 0U;
  std::size_t high = node.keys.size();
  while (low < high) {
    const std::size_t middle = low + (high - low) / 2U;
    if (node.keys[middle] < key) {
      low = middle + 1U;
    } else {
      high = middle;
    }
  }
  return low;
}

bool BTreeSet::contains(const Node& node, std::int64_t key) const {
  const std::size_t index = lower_bound_index(node, key);
  if (index < node.keys.size() && node.keys[index] == key) {
    return true;
  }
  if (node.leaf) {
    return false;
  }
  return contains(*node.children[index], key);
}

bool BTreeSet::contains(std::int64_t key) const {
  return contains(*root_, key);
}

void BTreeSet::split_child(Node& parent, std::size_t child_index) {
  Node& child = *parent.children[child_index];
  if (child.keys.size() != max_keys()) {
    throw std::logic_error("B-tree split requires a full child");
  }

  const std::int64_t median = child.keys[minimum_degree_ - 1U];
  auto sibling = std::make_unique<Node>(child.leaf);
  sibling->keys.insert(sibling->keys.end(),
                       child.keys.begin() +
                           static_cast<std::ptrdiff_t>(minimum_degree_),
                       child.keys.end());
  child.keys.resize(minimum_degree_ - 1U);

  if (!child.leaf) {
    sibling->children.reserve(minimum_degree_);
    for (std::size_t index = minimum_degree_; index < child.children.size();
         ++index) {
      sibling->children.push_back(std::move(child.children[index]));
    }
    child.children.resize(minimum_degree_);
  }

  parent.keys.insert(parent.keys.begin() +
                         static_cast<std::ptrdiff_t>(child_index),
                     median);
  parent.children.insert(parent.children.begin() +
                             static_cast<std::ptrdiff_t>(child_index + 1U),
                         std::move(sibling));
  ++diagnostics_.splits;
}

void BTreeSet::insert_non_full(Node& node, std::int64_t key) {
  std::size_t index = lower_bound_index(node, key);
  if (node.leaf) {
    node.keys.insert(node.keys.begin() + static_cast<std::ptrdiff_t>(index), key);
    return;
  }

  if (node.children[index]->keys.size() == max_keys()) {
    split_child(node, index);
    if (node.keys[index] < key) {
      ++index;
    }
  }
  insert_non_full(*node.children[index], key);
}

bool BTreeSet::insert(std::int64_t key) {
  if (contains(key)) {
    return false;
  }

  if (root_->keys.size() == max_keys()) {
    auto new_root = std::make_unique<Node>(false);
    new_root->children.push_back(std::move(root_));
    split_child(*new_root, 0U);
    root_ = std::move(new_root);
  }
  insert_non_full(*root_, key);
  ++size_;
  return true;
}

std::int64_t BTreeSet::subtree_minimum(const Node& node) const {
  const Node* current = &node;
  while (!current->leaf) {
    current = current->children.front().get();
  }
  return current->keys.front();
}

std::int64_t BTreeSet::subtree_maximum(const Node& node) const {
  const Node* current = &node;
  while (!current->leaf) {
    current = current->children.back().get();
  }
  return current->keys.back();
}

void BTreeSet::borrow_from_previous(Node& parent, std::size_t child_index) {
  Node& child = *parent.children[child_index];
  Node& sibling = *parent.children[child_index - 1U];

  child.keys.insert(child.keys.begin(), parent.keys[child_index - 1U]);
  parent.keys[child_index - 1U] = sibling.keys.back();
  sibling.keys.pop_back();

  if (!child.leaf) {
    child.children.insert(child.children.begin(),
                          std::move(sibling.children.back()));
    sibling.children.pop_back();
  }
  ++diagnostics_.borrows_from_previous;
}

void BTreeSet::borrow_from_next(Node& parent, std::size_t child_index) {
  Node& child = *parent.children[child_index];
  Node& sibling = *parent.children[child_index + 1U];

  child.keys.push_back(parent.keys[child_index]);
  parent.keys[child_index] = sibling.keys.front();
  sibling.keys.erase(sibling.keys.begin());

  if (!child.leaf) {
    child.children.push_back(std::move(sibling.children.front()));
    sibling.children.erase(sibling.children.begin());
  }
  ++diagnostics_.borrows_from_next;
}

void BTreeSet::merge_children(Node& parent, std::size_t key_index) {
  Node& left = *parent.children[key_index];
  std::unique_ptr<Node> right = std::move(parent.children[key_index + 1U]);

  left.keys.push_back(parent.keys[key_index]);
  left.keys.insert(left.keys.end(), right->keys.begin(), right->keys.end());
  if (!left.leaf) {
    for (auto& child : right->children) {
      left.children.push_back(std::move(child));
    }
  }

  parent.keys.erase(parent.keys.begin() +
                    static_cast<std::ptrdiff_t>(key_index));
  parent.children.erase(parent.children.begin() +
                        static_cast<std::ptrdiff_t>(key_index + 1U));
  ++diagnostics_.merges;
}

void BTreeSet::ensure_child_can_lose_key(Node& parent,
                                         std::size_t& child_index) {
  if (parent.children[child_index]->keys.size() >= minimum_degree_) {
    return;
  }
  if (child_index > 0U &&
      parent.children[child_index - 1U]->keys.size() >= minimum_degree_) {
    borrow_from_previous(parent, child_index);
    return;
  }
  if (child_index < parent.keys.size() &&
      parent.children[child_index + 1U]->keys.size() >= minimum_degree_) {
    borrow_from_next(parent, child_index);
    return;
  }
  if (child_index < parent.keys.size()) {
    merge_children(parent, child_index);
  } else {
    merge_children(parent, child_index - 1U);
    --child_index;
  }
}

void BTreeSet::erase_from_internal(Node& node, std::size_t key_index) {
  const std::int64_t key = node.keys[key_index];
  Node& left = *node.children[key_index];
  Node& right = *node.children[key_index + 1U];

  if (left.keys.size() >= minimum_degree_) {
    const std::int64_t predecessor = subtree_maximum(left);
    node.keys[key_index] = predecessor;
    erase_existing(left, predecessor);
    return;
  }
  if (right.keys.size() >= minimum_degree_) {
    const std::int64_t successor = subtree_minimum(right);
    node.keys[key_index] = successor;
    erase_existing(right, successor);
    return;
  }

  merge_children(node, key_index);
  erase_existing(*node.children[key_index], key);
}

void BTreeSet::erase_existing(Node& node, std::int64_t key) {
  std::size_t index = lower_bound_index(node, key);
  if (index < node.keys.size() && node.keys[index] == key) {
    if (node.leaf) {
      node.keys.erase(node.keys.begin() + static_cast<std::ptrdiff_t>(index));
    } else {
      erase_from_internal(node, index);
    }
    return;
  }

  if (node.leaf) {
    throw std::logic_error("B-tree erase lost an existing key");
  }

  ensure_child_can_lose_key(node, index);
  erase_existing(*node.children[index], key);
}

bool BTreeSet::erase(std::int64_t key) {
  if (!contains(key)) {
    return false;
  }

  erase_existing(*root_, key);
  --size_;
  if (!root_->leaf && root_->keys.empty()) {
    root_ = std::move(root_->children.front());
    ++diagnostics_.root_shrinks;
  }
  return true;
}

void BTreeSet::collect_in_order(const Node& node,
                                std::vector<std::int64_t>& out) const {
  if (node.leaf) {
    out.insert(out.end(), node.keys.begin(), node.keys.end());
    return;
  }
  for (std::size_t index = 0; index < node.keys.size(); ++index) {
    collect_in_order(*node.children[index], out);
    out.push_back(node.keys[index]);
  }
  collect_in_order(*node.children.back(), out);
}

std::vector<std::int64_t> BTreeSet::values_in_order() const {
  std::vector<std::int64_t> result;
  result.reserve(size_);
  collect_in_order(*root_, result);
  return result;
}

bool BTreeSet::valid_structure() const {
  if (!root_ || minimum_degree_ < 2U) {
    return false;
  }

  std::optional<std::size_t> leaf_depth;
  std::size_t key_count = 0U;
  const auto validate =
      [&](auto&& self, const Node& node, bool is_root, std::size_t depth,
          std::optional<std::int64_t> lower,
          std::optional<std::int64_t> upper) -> bool {
    if (node.keys.size() > max_keys()) {
      return false;
    }
    if (!is_root && node.keys.size() < minimum_degree_ - 1U) {
      return false;
    }
    if (is_root && !node.leaf && node.keys.empty()) {
      return false;
    }

    for (std::size_t index = 0; index < node.keys.size(); ++index) {
      const std::int64_t key = node.keys[index];
      if (index > 0U && !(node.keys[index - 1U] < key)) {
        return false;
      }
      if (lower.has_value() && !(lower.value() < key)) {
        return false;
      }
      if (upper.has_value() && !(key < upper.value())) {
        return false;
      }
    }

    key_count += node.keys.size();
    if (node.leaf) {
      if (!node.children.empty()) {
        return false;
      }
      if (!leaf_depth.has_value()) {
        leaf_depth = depth;
      }
      return leaf_depth.value() == depth;
    }

    if (node.children.size() != node.keys.size() + 1U) {
      return false;
    }
    for (std::size_t child_index = 0; child_index < node.children.size();
         ++child_index) {
      if (!node.children[child_index]) {
        return false;
      }
      const std::optional<std::int64_t> child_lower =
          child_index == 0U
              ? lower
              : std::optional<std::int64_t>{node.keys[child_index - 1U]};
      const std::optional<std::int64_t> child_upper =
          child_index == node.keys.size()
              ? upper
              : std::optional<std::int64_t>{node.keys[child_index]};
      if (!self(self, *node.children[child_index], false, depth + 1U,
                child_lower, child_upper)) {
        return false;
      }
    }
    return true;
  };

  return validate(validate, *root_, true, 0U, std::nullopt, std::nullopt) &&
         key_count == size_;
}

const BTreeMutationDiagnostics& BTreeSet::diagnostics() const noexcept {
  return diagnostics_;
}

}  // namespace algorithms::data_structures
