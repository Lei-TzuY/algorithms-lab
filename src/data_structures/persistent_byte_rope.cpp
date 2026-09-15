#include "algorithms/data_structures/persistent_byte_rope.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct detail::PersistentByteRopeNode {
  std::string bytes;
  std::shared_ptr<const PersistentByteRopeNode> left;
  std::shared_ptr<const PersistentByteRopeNode> right;
  std::size_t size = 0;
  std::size_t height = 0;
  std::size_t leaf_count = 0;
  [[nodiscard]] bool leaf() const noexcept { return !left && !right; }
};

namespace {
using Rope = PersistentByteRope;
using Node = detail::PersistentByteRopeNode;
using NodePtr = std::shared_ptr<const Node>;

struct Validation {
  bool valid = true;
  std::size_t size = 0;
  std::size_t height = 0;
  std::size_t leaves = 0;
};

std::size_t height(const NodePtr& node) noexcept { return node ? node->height : 0; }

std::size_t checked_add(std::size_t a, std::size_t b, const char* message) {
  if (a > std::numeric_limits<std::size_t>::max() - b) {
    throw std::length_error(message);
  }
  return a + b;
}

NodePtr leaf(std::string_view bytes) {
  if (bytes.empty() || bytes.size() > Rope::kMaxLeafBytes) {
    throw std::logic_error("invalid rope leaf size");
  }
  auto node = std::make_shared<Node>();
  node->bytes.assign(bytes.begin(), bytes.end());
  node->size = bytes.size();
  node->height = 1;
  node->leaf_count = 1;
  return node;
}

NodePtr internal(const NodePtr& left, const NodePtr& right) {
  if (!left) return right;
  if (!right) return left;
  auto node = std::make_shared<Node>();
  node->left = left;
  node->right = right;
  node->size = checked_add(left->size, right->size,
                           "rope length is not representable");
  node->leaf_count = checked_add(left->leaf_count, right->leaf_count,
                                 "rope leaf count is not representable");
  node->height = checked_add(std::max(left->height, right->height), 1,
                             "rope height is not representable");
  return node;
}

NodePtr rotate_left(const NodePtr& node) {
  const NodePtr& pivot = node->right;
  if (!pivot || pivot->leaf()) throw std::logic_error("invalid left rotation");
  return internal(internal(node->left, pivot->left), pivot->right);
}

NodePtr rotate_right(const NodePtr& node) {
  const NodePtr& pivot = node->left;
  if (!pivot || pivot->leaf()) throw std::logic_error("invalid right rotation");
  return internal(pivot->left, internal(pivot->right, node->right));
}

NodePtr rebalance(const NodePtr& node) {
  if (!node || node->leaf()) return node;
  const auto lh = height(node->left);
  const auto rh = height(node->right);
  if (lh > rh + 1) {
    const NodePtr& left = node->left;
    if (height(left->right) > height(left->left)) {
      return rotate_right(internal(rotate_left(left), node->right));
    }
    return rotate_right(node);
  }
  if (rh > lh + 1) {
    const NodePtr& right = node->right;
    if (height(right->left) > height(right->right)) {
      return rotate_left(internal(node->left, rotate_right(right)));
    }
    return rotate_left(node);
  }
  return node;
}

NodePtr join(const NodePtr& left, const NodePtr& right) {
  if (!left) return right;
  if (!right) return left;
  if (left->height > right->height + 1) {
    if (left->leaf()) throw std::logic_error("rope height invariant violated");
    return rebalance(internal(left->left, join(left->right, right)));
  }
  if (right->height > left->height + 1) {
    if (right->leaf()) throw std::logic_error("rope height invariant violated");
    return rebalance(internal(join(left, right->left), right->right));
  }
  return internal(left, right);
}

NodePtr build_balanced(const std::vector<NodePtr>& leaves, std::size_t begin,
                       std::size_t end) {
  if (begin == end) return nullptr;
  if (end - begin == 1) return leaves[begin];
  const std::size_t middle = begin + (end - begin) / 2;
  return internal(build_balanced(leaves, begin, middle),
                  build_balanced(leaves, middle, end));
}

NodePtr build(std::string_view bytes) {
  if (bytes.empty()) return nullptr;
  std::vector<NodePtr> leaves;
  leaves.reserve(1 + (bytes.size() - 1) / Rope::kMaxLeafBytes);
  for (std::size_t offset = 0; offset < bytes.size();) {
    const std::size_t count =
        std::min(Rope::kMaxLeafBytes, bytes.size() - offset);
    leaves.push_back(leaf(bytes.substr(offset, count)));
    offset += count;
  }
  return build_balanced(leaves, 0, leaves.size());
}

std::pair<NodePtr, NodePtr> split_node(const NodePtr& node,
                                       std::size_t index) {
  if (!node) {
    if (index != 0) throw std::out_of_range("rope split index out of range");
    return {nullptr, nullptr};
  }
  if (index > node->size) throw std::out_of_range("rope split index out of range");
  if (index == 0) return {nullptr, node};
  if (index == node->size) return {node, nullptr};
  if (node->leaf()) {
    return {leaf(std::string_view(node->bytes).substr(0, index)),
            leaf(std::string_view(node->bytes).substr(index))};
  }
  const auto left_size = node->left->size;
  if (index < left_size) {
    auto [a, b] = split_node(node->left, index);
    return {a, join(b, node->right)};
  }
  if (index == left_size) return {node->left, node->right};
  auto [a, b] = split_node(node->right, index - left_size);
  return {join(node->left, a), b};
}

void append(const NodePtr& node, std::string& output) {
  if (!node) return;
  if (node->leaf()) {
    output.append(node->bytes);
    return;
  }
  append(node->left, output);
  append(node->right, output);
}

Validation validate(const NodePtr& node) {
  if (!node) return {};
  if (node->leaf()) {
    const bool ok = !node->bytes.empty() &&
                    node->bytes.size() <= Rope::kMaxLeafBytes &&
                    node->size == node->bytes.size() && node->height == 1 &&
                    node->leaf_count == 1;
    return {ok, node->bytes.size(), 1, 1};
  }
  if (!node->bytes.empty() || !node->left || !node->right) {
    return {false, 0, 0, 0};
  }
  const Validation left = validate(node->left);
  const Validation right = validate(node->right);
  if (!left.valid || !right.valid) return {false, 0, 0, 0};
  if (left.height > right.height + 1 || right.height > left.height + 1) {
    return {false, 0, 0, 0};
  }
  if (left.size > std::numeric_limits<std::size_t>::max() - right.size ||
      left.leaves > std::numeric_limits<std::size_t>::max() - right.leaves) {
    return {false, 0, 0, 0};
  }
  const auto total = left.size + right.size;
  const auto leaves = left.leaves + right.leaves;
  const auto h = 1 + std::max(left.height, right.height);
  return {node->size == total && node->leaf_count == leaves &&
              node->height == h,
          total, h, leaves};
}

void collect(const NodePtr& node, std::unordered_set<const Node*>& nodes) {
  if (!node || !nodes.insert(node.get()).second) return;
  collect(node->left, nodes);
  collect(node->right, nodes);
}
}  // namespace

PersistentByteRope::PersistentByteRope(std::string_view bytes)
    : root_(build(bytes)) {}
PersistentByteRope::PersistentByteRope(NodePtr root) : root_(std::move(root)) {}
bool PersistentByteRope::empty() const noexcept { return !root_; }
std::size_t PersistentByteRope::size() const noexcept {
  return root_ ? root_->size : 0;
}
std::size_t PersistentByteRope::height() const noexcept {
  return root_ ? root_->height : 0;
}
std::size_t PersistentByteRope::leaf_count() const noexcept {
  return root_ ? root_->leaf_count : 0;
}

std::uint8_t PersistentByteRope::at(std::size_t index) const {
  if (index >= size()) throw std::out_of_range("rope index out of range");
  const detail::PersistentByteRopeNode* node = root_.get();
  std::size_t offset = index;
  while (!node->leaf()) {
    if (offset < node->left->size) {
      node = node->left.get();
    } else {
      offset -= node->left->size;
      node = node->right.get();
    }
  }
  return static_cast<std::uint8_t>(
      static_cast<unsigned char>(node->bytes[offset]));
}

std::string PersistentByteRope::to_string() const {
  std::string result;
  result.reserve(size());
  append(root_, result);
  return result;
}

PersistentByteRope PersistentByteRope::concat(
    const PersistentByteRope& first, const PersistentByteRope& second) {
  return PersistentByteRope(join(first.root_, second.root_));
}

std::pair<PersistentByteRope, PersistentByteRope> PersistentByteRope::split(
    std::size_t index) const {
  if (index > size()) throw std::out_of_range("rope split index out of range");
  auto [left, right] = split_node(root_, index);
  return {PersistentByteRope(std::move(left)),
          PersistentByteRope(std::move(right))};
}

PersistentByteRope PersistentByteRope::insert(std::size_t index,
                                              std::string_view bytes) const {
  if (index > size()) throw std::out_of_range("rope insert index out of range");
  if (bytes.empty()) return *this;
  auto [left, right] = split_node(root_, index);
  return PersistentByteRope(join(join(left, build(bytes)), right));
}

PersistentByteRope PersistentByteRope::erase(std::size_t begin,
                                             std::size_t end) const {
  if (begin > end || end > size()) {
    throw std::out_of_range("rope range out of bounds");
  }
  if (begin == end) return *this;
  auto [left, suffix] = split_node(root_, begin);
  auto [discarded, right] = split_node(suffix, end - begin);
  static_cast<void>(discarded);
  return PersistentByteRope(join(left, right));
}

PersistentByteRope PersistentByteRope::slice(std::size_t begin,
                                             std::size_t end) const {
  if (begin > end || end > size()) {
    throw std::out_of_range("rope range out of bounds");
  }
  if (begin == 0 && end == size()) return *this;
  auto [discarded_left, suffix] = split_node(root_, begin);
  static_cast<void>(discarded_left);
  auto [middle, discarded_right] = split_node(suffix, end - begin);
  static_cast<void>(discarded_right);
  return PersistentByteRope(std::move(middle));
}

bool PersistentByteRope::valid_structure() const noexcept {
  try {
    return validate(root_).valid;
  } catch (...) {
    return false;
  }
}

std::size_t PersistentByteRope::unique_node_count() const {
  std::unordered_set<const detail::PersistentByteRopeNode*> nodes;
  collect(root_, nodes);
  return nodes.size();
}

std::size_t PersistentByteRope::shared_node_count_with(
    const PersistentByteRope& other) const {
  std::unordered_set<const detail::PersistentByteRopeNode*> first;
  std::unordered_set<const detail::PersistentByteRopeNode*> second;
  collect(root_, first);
  collect(other.root_, second);
  std::size_t shared = 0;
  for (const detail::PersistentByteRopeNode* node : first) {
    if (second.contains(node)) ++shared;
  }
  return shared;
}

}  // namespace algorithms::data_structures
