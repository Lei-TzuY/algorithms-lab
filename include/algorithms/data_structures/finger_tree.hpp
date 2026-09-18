#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Persistent cardinality-measured finger tree specialized to int64_t values.
//
// End updates return a new tree and leave every prior version unchanged. The
// implementation uses immutable 1..4 element digits and recursive 2/3 nodes.
// It intentionally exposes only the cardinality measure in this recovery slice;
// generic monoid measures, split/search and concatenation are separate surfaces.
class PersistentFingerTree {
 public:
  PersistentFingerTree() : root_(make_empty(0U)) {}

  [[nodiscard]] bool empty() const noexcept { return root_->measure == 0U; }
  [[nodiscard]] std::size_t size() const noexcept { return root_->measure; }

  [[nodiscard]] std::int64_t front() const {
    if (empty()) {
      throw std::out_of_range("finger tree front on empty sequence");
    }
    return lookup_tree(*root_, 0U);
  }

  [[nodiscard]] std::int64_t back() const {
    if (empty()) {
      throw std::out_of_range("finger tree back on empty sequence");
    }
    return lookup_tree(*root_, size() - 1U);
  }

  [[nodiscard]] std::int64_t at(const std::size_t index) const {
    if (index >= size()) {
      throw std::out_of_range("finger tree index out of range");
    }
    return lookup_tree(*root_, index);
  }

  [[nodiscard]] PersistentFingerTree push_front(const std::int64_t value) const {
    return PersistentFingerTree(prepend(make_leaf(value), root_));
  }

  [[nodiscard]] PersistentFingerTree push_back(const std::int64_t value) const {
    return PersistentFingerTree(append(root_, make_leaf(value)));
  }

  [[nodiscard]] PersistentFingerTree pop_front() const {
    if (empty()) {
      throw std::out_of_range("finger tree pop_front on empty sequence");
    }
    return PersistentFingerTree(view_left(root_).rest);
  }

  [[nodiscard]] PersistentFingerTree pop_back() const {
    if (empty()) {
      throw std::out_of_range("finger tree pop_back on empty sequence");
    }
    return PersistentFingerTree(view_right(root_).rest);
  }

  [[nodiscard]] std::vector<std::int64_t> to_vector() const {
    std::vector<std::int64_t> result;
    result.reserve(size());
    append_tree_values(*root_, result);
    return result;
  }

  [[nodiscard]] bool valid_invariants() const noexcept {
    std::size_t measure = 0U;
    return validate_tree(root_, 0U, measure) && measure == root_->measure;
  }

 private:
  struct Element;
  struct Tree;

  using ElementPtr = std::shared_ptr<const Element>;
  using TreePtr = std::shared_ptr<const Tree>;

  enum class TreeKind : unsigned char { empty, single, deep };

  struct Element {
    bool leaf{false};
    std::int64_t value{0};
    std::array<ElementPtr, 3U> children{};
    std::size_t child_count{0U};
    std::size_t height{0U};
    std::size_t measure{0U};
  };

  struct Tree {
    TreeKind kind{TreeKind::empty};
    std::size_t level{0U};
    std::size_t measure{0U};
    ElementPtr single;
    std::vector<ElementPtr> prefix;
    TreePtr middle;
    std::vector<ElementPtr> suffix;
  };

  struct View {
    ElementPtr element;
    TreePtr rest;
  };

  explicit PersistentFingerTree(TreePtr root) : root_(std::move(root)) {
    if (!root_ || root_->level != 0U) {
      throw std::logic_error("finger tree root level invariant violated");
    }
  }

  static std::size_t checked_add(const std::size_t first,
                                 const std::size_t second) {
    if (second > std::numeric_limits<std::size_t>::max() - first) {
      throw std::length_error("finger tree measure overflow");
    }
    return first + second;
  }

  static ElementPtr make_leaf(const std::int64_t value) {
    auto element = std::make_shared<Element>();
    element->leaf = true;
    element->value = value;
    element->height = 0U;
    element->measure = 1U;
    return element;
  }

  static ElementPtr make_node(const std::vector<ElementPtr>& children) {
    if (children.size() < 2U || children.size() > 3U || !children.front()) {
      throw std::logic_error("finger tree internal node arity invariant violated");
    }
    const std::size_t child_height = children.front()->height;
    if (child_height == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("finger tree element height overflow");
    }

    auto element = std::make_shared<Element>();
    element->leaf = false;
    element->child_count = children.size();
    element->height = child_height + 1U;
    for (std::size_t index = 0U; index < children.size(); ++index) {
      if (!children[index] || children[index]->height != child_height) {
        throw std::logic_error("finger tree internal node height mismatch");
      }
      element->children[index] = children[index];
      element->measure = checked_add(element->measure, children[index]->measure);
    }
    return element;
  }

  static TreePtr make_empty(const std::size_t level) {
    auto tree = std::make_shared<Tree>();
    tree->kind = TreeKind::empty;
    tree->level = level;
    return tree;
  }

  static TreePtr make_single(const std::size_t level, const ElementPtr& element) {
    if (!element || element->height != level) {
      throw std::logic_error("finger tree single level mismatch");
    }
    auto tree = std::make_shared<Tree>();
    tree->kind = TreeKind::single;
    tree->level = level;
    tree->measure = element->measure;
    tree->single = element;
    return tree;
  }

  static std::size_t digit_measure(const std::vector<ElementPtr>& digit,
                                   const std::size_t level) {
    if (digit.empty() || digit.size() > 4U) {
      throw std::logic_error("finger tree digit size invariant violated");
    }
    std::size_t measure = 0U;
    for (const auto& element : digit) {
      if (!element || element->height != level) {
        throw std::logic_error("finger tree digit level mismatch");
      }
      measure = checked_add(measure, element->measure);
    }
    return measure;
  }

  static TreePtr make_deep(const std::size_t level,
                           std::vector<ElementPtr> prefix,
                           TreePtr middle,
                           std::vector<ElementPtr> suffix) {
    if (!middle || level == std::numeric_limits<std::size_t>::max() ||
        middle->level != level + 1U) {
      throw std::logic_error("finger tree middle level mismatch");
    }
    const std::size_t prefix_measure = digit_measure(prefix, level);
    const std::size_t suffix_measure = digit_measure(suffix, level);

    auto tree = std::make_shared<Tree>();
    tree->kind = TreeKind::deep;
    tree->level = level;
    tree->prefix = std::move(prefix);
    tree->middle = std::move(middle);
    tree->suffix = std::move(suffix);
    tree->measure = checked_add(
        checked_add(prefix_measure, tree->middle->measure), suffix_measure);
    return tree;
  }

  static TreePtr from_digit(const std::size_t level,
                            const std::vector<ElementPtr>& digit) {
    if (digit.empty() || digit.size() > 4U) {
      throw std::logic_error("finger tree collapse digit invariant violated");
    }
    if (digit.size() == 1U) {
      return make_single(level, digit.front());
    }
    std::vector<ElementPtr> prefix{digit.front()};
    std::vector<ElementPtr> suffix(digit.begin() + 1, digit.end());
    return make_deep(level, std::move(prefix), make_empty(level + 1U),
                     std::move(suffix));
  }

  static TreePtr prepend(const ElementPtr& element, const TreePtr& tree) {
    if (!tree || !element || element->height != tree->level) {
      throw std::logic_error("finger tree prepend level mismatch");
    }
    if (tree->kind == TreeKind::empty) {
      return make_single(tree->level, element);
    }
    if (tree->kind == TreeKind::single) {
      return make_deep(tree->level, {element}, make_empty(tree->level + 1U),
                       {tree->single});
    }

    if (tree->prefix.size() < 4U) {
      std::vector<ElementPtr> prefix;
      prefix.reserve(tree->prefix.size() + 1U);
      prefix.push_back(element);
      prefix.insert(prefix.end(), tree->prefix.begin(), tree->prefix.end());
      return make_deep(tree->level, std::move(prefix), tree->middle,
                       tree->suffix);
    }

    const ElementPtr promoted =
        make_node({tree->prefix[1U], tree->prefix[2U], tree->prefix[3U]});
    return make_deep(tree->level, {element, tree->prefix[0U]},
                     prepend(promoted, tree->middle), tree->suffix);
  }

  static TreePtr append(const TreePtr& tree, const ElementPtr& element) {
    if (!tree || !element || element->height != tree->level) {
      throw std::logic_error("finger tree append level mismatch");
    }
    if (tree->kind == TreeKind::empty) {
      return make_single(tree->level, element);
    }
    if (tree->kind == TreeKind::single) {
      return make_deep(tree->level, {tree->single}, make_empty(tree->level + 1U),
                       {element});
    }

    if (tree->suffix.size() < 4U) {
      std::vector<ElementPtr> suffix = tree->suffix;
      suffix.push_back(element);
      return make_deep(tree->level, tree->prefix, tree->middle,
                       std::move(suffix));
    }

    const ElementPtr promoted =
        make_node({tree->suffix[0U], tree->suffix[1U], tree->suffix[2U]});
    return make_deep(tree->level, tree->prefix,
                     append(tree->middle, promoted),
                     {tree->suffix[3U], element});
  }

  static std::vector<ElementPtr> node_children(const ElementPtr& node,
                                               const std::size_t child_height) {
    if (!node || node->leaf || node->height != child_height + 1U ||
        (node->child_count != 2U && node->child_count != 3U)) {
      throw std::logic_error("finger tree promoted node invariant violated");
    }
    std::vector<ElementPtr> result;
    result.reserve(node->child_count);
    for (std::size_t index = 0U; index < node->child_count; ++index) {
      result.push_back(node->children[index]);
    }
    return result;
  }

  static View view_left(const TreePtr& tree) {
    if (!tree || tree->kind == TreeKind::empty) {
      throw std::logic_error("finger tree view_left on empty tree");
    }
    if (tree->kind == TreeKind::single) {
      return {tree->single, make_empty(tree->level)};
    }

    const ElementPtr first = tree->prefix.front();
    if (tree->prefix.size() > 1U) {
      std::vector<ElementPtr> prefix(tree->prefix.begin() + 1,
                                     tree->prefix.end());
      return {first, make_deep(tree->level, std::move(prefix), tree->middle,
                               tree->suffix)};
    }

    if (tree->middle->kind == TreeKind::empty) {
      return {first, from_digit(tree->level, tree->suffix)};
    }

    const View middle_view = view_left(tree->middle);
    return {first,
            make_deep(tree->level,
                      node_children(middle_view.element, tree->level),
                      middle_view.rest, tree->suffix)};
  }

  static View view_right(const TreePtr& tree) {
    if (!tree || tree->kind == TreeKind::empty) {
      throw std::logic_error("finger tree view_right on empty tree");
    }
    if (tree->kind == TreeKind::single) {
      return {tree->single, make_empty(tree->level)};
    }

    const ElementPtr last = tree->suffix.back();
    if (tree->suffix.size() > 1U) {
      std::vector<ElementPtr> suffix(tree->suffix.begin(),
                                     tree->suffix.end() - 1);
      return {last, make_deep(tree->level, tree->prefix, tree->middle,
                              std::move(suffix))};
    }

    if (tree->middle->kind == TreeKind::empty) {
      return {last, from_digit(tree->level, tree->prefix)};
    }

    const View middle_view = view_right(tree->middle);
    return {last,
            make_deep(tree->level, tree->prefix, middle_view.rest,
                      node_children(middle_view.element, tree->level))};
  }

  static std::int64_t lookup_element(const Element& element,
                                     std::size_t index) {
    if (index >= element.measure) {
      throw std::logic_error("finger tree element lookup invariant violated");
    }
    if (element.leaf) {
      if (index != 0U) {
        throw std::logic_error("finger tree leaf lookup invariant violated");
      }
      return element.value;
    }
    for (std::size_t child_index = 0U; child_index < element.child_count;
         ++child_index) {
      const ElementPtr& child = element.children[child_index];
      if (index < child->measure) {
        return lookup_element(*child, index);
      }
      index -= child->measure;
    }
    throw std::logic_error("finger tree internal lookup fell through");
  }

  static std::int64_t lookup_tree(const Tree& tree, std::size_t index) {
    if (index >= tree.measure) {
      throw std::logic_error("finger tree tree lookup invariant violated");
    }
    if (tree.kind == TreeKind::single) {
      return lookup_element(*tree.single, index);
    }
    if (tree.kind != TreeKind::deep) {
      throw std::logic_error("finger tree nonempty lookup kind invariant violated");
    }

    for (const auto& element : tree.prefix) {
      if (index < element->measure) {
        return lookup_element(*element, index);
      }
      index -= element->measure;
    }
    if (index < tree.middle->measure) {
      return lookup_tree(*tree.middle, index);
    }
    index -= tree.middle->measure;
    for (const auto& element : tree.suffix) {
      if (index < element->measure) {
        return lookup_element(*element, index);
      }
      index -= element->measure;
    }
    throw std::logic_error("finger tree lookup fell through");
  }

  static void append_element_values(const Element& element,
                                    std::vector<std::int64_t>& output) {
    if (element.leaf) {
      output.push_back(element.value);
      return;
    }
    for (std::size_t index = 0U; index < element.child_count; ++index) {
      append_element_values(*element.children[index], output);
    }
  }

  static void append_tree_values(const Tree& tree,
                                 std::vector<std::int64_t>& output) {
    if (tree.kind == TreeKind::empty) {
      return;
    }
    if (tree.kind == TreeKind::single) {
      append_element_values(*tree.single, output);
      return;
    }
    for (const auto& element : tree.prefix) {
      append_element_values(*element, output);
    }
    append_tree_values(*tree.middle, output);
    for (const auto& element : tree.suffix) {
      append_element_values(*element, output);
    }
  }

  static bool validate_element(const ElementPtr& element,
                               const std::size_t expected_height,
                               std::size_t& measure) noexcept {
    if (!element || element->height != expected_height) {
      return false;
    }
    if (element->leaf) {
      measure = 1U;
      return expected_height == 0U && element->measure == 1U &&
             element->child_count == 0U && !element->children[0U] &&
             !element->children[1U] && !element->children[2U];
    }
    if (expected_height == 0U ||
        (element->child_count != 2U && element->child_count != 3U)) {
      return false;
    }
    std::size_t total = 0U;
    for (std::size_t index = 0U; index < element->child_count; ++index) {
      std::size_t child_measure = 0U;
      if (!validate_element(element->children[index], expected_height - 1U,
                            child_measure) ||
          child_measure > std::numeric_limits<std::size_t>::max() - total) {
        return false;
      }
      total += child_measure;
    }
    for (std::size_t index = element->child_count; index < 3U; ++index) {
      if (element->children[index]) {
        return false;
      }
    }
    measure = total;
    return element->measure == total;
  }

  static bool validate_digit(const std::vector<ElementPtr>& digit,
                             const std::size_t level,
                             std::size_t& measure) noexcept {
    if (digit.empty() || digit.size() > 4U) {
      return false;
    }
    std::size_t total = 0U;
    for (const auto& element : digit) {
      std::size_t element_measure = 0U;
      if (!validate_element(element, level, element_measure) ||
          element_measure > std::numeric_limits<std::size_t>::max() - total) {
        return false;
      }
      total += element_measure;
    }
    measure = total;
    return true;
  }

  static bool validate_tree(const TreePtr& tree, const std::size_t expected_level,
                            std::size_t& measure) noexcept {
    if (!tree || tree->level != expected_level) {
      return false;
    }
    if (tree->kind == TreeKind::empty) {
      measure = 0U;
      return tree->measure == 0U && !tree->single && tree->prefix.empty() &&
             !tree->middle && tree->suffix.empty();
    }
    if (tree->kind == TreeKind::single) {
      std::size_t element_measure = 0U;
      if (!tree->single || !tree->prefix.empty() || tree->middle ||
          !tree->suffix.empty() ||
          !validate_element(tree->single, expected_level, element_measure)) {
        return false;
      }
      measure = element_measure;
      return tree->measure == element_measure;
    }

    if (expected_level == std::numeric_limits<std::size_t>::max() ||
        tree->single || !tree->middle ||
        tree->middle->level != expected_level + 1U) {
      return false;
    }
    std::size_t prefix_measure = 0U;
    std::size_t middle_measure = 0U;
    std::size_t suffix_measure = 0U;
    if (!validate_digit(tree->prefix, expected_level, prefix_measure) ||
        !validate_tree(tree->middle, expected_level + 1U, middle_measure) ||
        !validate_digit(tree->suffix, expected_level, suffix_measure)) {
      return false;
    }
    if (middle_measure > std::numeric_limits<std::size_t>::max() -
                             prefix_measure) {
      return false;
    }
    const std::size_t first = prefix_measure + middle_measure;
    if (suffix_measure > std::numeric_limits<std::size_t>::max() - first) {
      return false;
    }
    measure = first + suffix_measure;
    return tree->measure == measure;
  }

  TreePtr root_;
};

}  // namespace algorithms::data_structures
