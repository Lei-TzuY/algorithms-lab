#include "algorithms/graphs/link_cut_tree.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

static_assert(std::numeric_limits<std::size_t>::digits <= 64,
              "LinkCutForest exact aggregate assumes size_t is at most 64 bits");

LinkCutForest::LinkCutForest(std::size_t vertex_count) : nodes_(vertex_count) {}

void LinkCutForest::validate_vertex(Vertex vertex) const {
  if (vertex >= nodes_.size()) {
    throw std::out_of_range("link-cut forest vertex out of range");
  }
}

bool LinkCutForest::is_auxiliary_root(Vertex vertex) const noexcept {
  const Vertex parent = nodes_[vertex].parent;
  if (parent == kNone) return true;
  return nodes_[parent].left != vertex && nodes_[parent].right != vertex;
}

std::size_t LinkCutForest::child_size(Vertex vertex) const noexcept {
  return vertex == kNone ? 0 : nodes_[vertex].auxiliary_size;
}

std::size_t LinkCutForest::child_represented_size(Vertex vertex) const noexcept {
  return vertex == kNone ? 0 : nodes_[vertex].represented_size;
}

LinkCutForest::ExactSum LinkCutForest::child_sum(Vertex vertex) const noexcept {
  return vertex == kNone ? ExactSum{} : nodes_[vertex].auxiliary_sum;
}

LinkCutForest::ExactSum LinkCutForest::child_auxiliary_virtual_sum(
    Vertex vertex) const noexcept {
  return vertex == kNone ? ExactSum{} : nodes_[vertex].auxiliary_virtual_sum;
}

LinkCutForest::ExactSum LinkCutForest::represented_sum(
    Vertex vertex) const noexcept {
  if (vertex == kNone) return ExactSum{};
  return add_exact(nodes_[vertex].auxiliary_sum,
                   nodes_[vertex].auxiliary_virtual_sum);
}

LinkCutForest::ExactSum LinkCutForest::child_represented_sum(
    Vertex vertex) const noexcept {
  return represented_sum(vertex);
}

LinkCutForest::ExactSum LinkCutForest::exact_from_value(
    std::int64_t value) noexcept {
  if (value >= 0) return ExactSum{static_cast<std::uint64_t>(value), 0, false};
  std::uint64_t magnitude = static_cast<std::uint64_t>(-(value + 1));
  ++magnitude;
  return ExactSum{magnitude, 0, true};
}

int LinkCutForest::compare_magnitude(const ExactSum& first,
                                     const ExactSum& second) noexcept {
  if (first.high != second.high) return first.high < second.high ? -1 : 1;
  if (first.low != second.low) return first.low < second.low ? -1 : 1;
  return 0;
}

LinkCutForest::ExactSum LinkCutForest::add_magnitude(
    const ExactSum& first, const ExactSum& second) noexcept {
  const std::uint64_t low = first.low + second.low;
  const std::uint64_t carry = low < first.low ? 1U : 0U;
  const std::uint64_t high = first.high + second.high + carry;
  return ExactSum{low, high, false};
}

LinkCutForest::ExactSum LinkCutForest::subtract_magnitude(
    const ExactSum& larger, const ExactSum& smaller) noexcept {
  const std::uint64_t borrow = larger.low < smaller.low ? 1U : 0U;
  const std::uint64_t low = larger.low - smaller.low;
  const std::uint64_t high = larger.high - smaller.high - borrow;
  return ExactSum{low, high, false};
}

LinkCutForest::ExactSum LinkCutForest::add_exact(
    const ExactSum& first, const ExactSum& second) noexcept {
  if (first.negative == second.negative) {
    ExactSum result = add_magnitude(first, second);
    result.negative = first.negative && (result.low != 0 || result.high != 0);
    return result;
  }
  const int comparison = compare_magnitude(first, second);
  if (comparison == 0) return ExactSum{};
  if (comparison > 0) {
    ExactSum result = subtract_magnitude(first, second);
    result.negative = first.negative;
    return result;
  }
  ExactSum result = subtract_magnitude(second, first);
  result.negative = second.negative;
  return result;
}

LinkCutForest::ExactSum LinkCutForest::negate_exact(ExactSum value) noexcept {
  if (!exact_is_zero(value)) value.negative = !value.negative;
  return value;
}

LinkCutForest::ExactSum LinkCutForest::subtract_exact(
    const ExactSum& first, const ExactSum& second) noexcept {
  return add_exact(first, negate_exact(second));
}

LinkCutForest::ExactSum LinkCutForest::scale_exact(
    const ExactSum& value, std::size_t count) noexcept {
  ExactSum addend = value;
  const bool negative = addend.negative;
  addend.negative = false;
  ExactSum result{};
  std::uint64_t multiplier = static_cast<std::uint64_t>(count);
  while (multiplier != 0) {
    if ((multiplier & 1U) != 0U) result = add_magnitude(result, addend);
    multiplier >>= 1U;
    if (multiplier != 0) addend = add_magnitude(addend, addend);
  }
  result.negative = negative && (result.low != 0 || result.high != 0);
  return result;
}

LinkCutForest::ExactSum LinkCutForest::scale_exact_value(
    std::int64_t value, std::size_t count) noexcept {
  return scale_exact(exact_from_value(value), count);
}

bool LinkCutForest::exact_equal(const ExactSum& first,
                                const ExactSum& second) noexcept {
  return first.low == second.low && first.high == second.high &&
         first.negative == second.negative;
}

bool LinkCutForest::exact_is_zero(const ExactSum& value) noexcept {
  return value.low == 0 && value.high == 0;
}

bool LinkCutForest::exact_fits_int64(const ExactSum& value) noexcept {
  if (value.high != 0) return false;
  constexpr std::uint64_t kNegativeLimit = std::uint64_t{1} << 63U;
  constexpr std::uint64_t kPositiveLimit = kNegativeLimit - 1U;
  return value.negative ? value.low <= kNegativeLimit : value.low <= kPositiveLimit;
}

std::int64_t LinkCutForest::narrow_exact_unchecked(
    const ExactSum& value) noexcept {
  constexpr std::uint64_t kNegativeLimit = std::uint64_t{1} << 63U;
  if (!value.negative) return static_cast<std::int64_t>(value.low);
  if (value.low == kNegativeLimit) return std::numeric_limits<std::int64_t>::min();
  return -static_cast<std::int64_t>(value.low);
}

std::int64_t LinkCutForest::narrow_exact(const ExactSum& value) {
  if (!exact_fits_int64(value)) {
    throw std::overflow_error("link-cut sum is outside int64");
  }
  return narrow_exact_unchecked(value);
}

bool LinkCutForest::can_add_int64(std::int64_t value,
                                  std::int64_t delta) noexcept {
  if (delta > 0) return value <= std::numeric_limits<std::int64_t>::max() - delta;
  if (delta < 0) return value >= std::numeric_limits<std::int64_t>::min() - delta;
  return true;
}

std::int64_t LinkCutForest::add_exact_to_int64(
    std::int64_t value, const ExactSum& delta) {
  const ExactSum result = add_exact(exact_from_value(value), delta);
  if (!exact_fits_int64(result)) {
    throw std::overflow_error("link-cut path addition would overflow int64");
  }
  return narrow_exact_unchecked(result);
}

void LinkCutForest::pull(Vertex vertex) {
  Node& node = nodes_[vertex];
  node.auxiliary_size = 1 + child_size(node.left) + child_size(node.right);
  node.represented_size = 1 + node.virtual_size +
                          child_represented_size(node.left) +
                          child_represented_size(node.right);
  node.auxiliary_virtual_sum = add_exact(
      node.virtual_sum,
      add_exact(child_auxiliary_virtual_sum(node.left),
                child_auxiliary_virtual_sum(node.right)));

  if (node.has_assignment) {
    node.auxiliary_sum = scale_exact_value(node.assignment_value, node.auxiliary_size);
    node.auxiliary_min = node.assignment_value;
    node.auxiliary_max = node.assignment_value;
    return;
  }

  ExactSum sum = exact_from_value(node.value);
  std::int64_t minimum = node.value;
  std::int64_t maximum = node.value;
  const bool shifted_children = !exact_is_zero(node.pending_addition);

  const auto include_child = [&](Vertex child) {
    if (child == kNone) return;
    ExactSum sum_part = nodes_[child].auxiliary_sum;
    std::int64_t child_minimum = nodes_[child].auxiliary_min;
    std::int64_t child_maximum = nodes_[child].auxiliary_max;
    if (shifted_children) {
      sum_part = add_exact(
          sum_part, scale_exact(node.pending_addition,
                                nodes_[child].auxiliary_size));
      child_minimum = add_exact_to_int64(child_minimum, node.pending_addition);
      child_maximum = add_exact_to_int64(child_maximum, node.pending_addition);
    }
    sum = add_exact(sum, sum_part);
    minimum = std::min(minimum, child_minimum);
    maximum = std::max(maximum, child_maximum);
  };
  include_child(node.left);
  include_child(node.right);
  node.auxiliary_sum = sum;
  node.auxiliary_min = minimum;
  node.auxiliary_max = maximum;
}

void LinkCutForest::apply_assignment(Vertex vertex,
                                     std::int64_t value) noexcept {
  if (vertex == kNone) return;
  Node& node = nodes_[vertex];
  node.value = value;
  node.auxiliary_sum = scale_exact_value(value, node.auxiliary_size);
  node.auxiliary_min = value;
  node.auxiliary_max = value;
  node.has_assignment = true;
  node.assignment_value = value;
  node.pending_addition = ExactSum{};
}

void LinkCutForest::apply_addition(Vertex vertex, const ExactSum& delta) {
  if (vertex == kNone || exact_is_zero(delta)) return;
  Node& node = nodes_[vertex];
  if (node.has_assignment) {
    const std::int64_t assigned = add_exact_to_int64(node.assignment_value, delta);
    apply_assignment(vertex, assigned);
    return;
  }
  node.value = add_exact_to_int64(node.value, delta);
  node.auxiliary_min = add_exact_to_int64(node.auxiliary_min, delta);
  node.auxiliary_max = add_exact_to_int64(node.auxiliary_max, delta);
  node.auxiliary_sum = add_exact(node.auxiliary_sum,
                                 scale_exact(delta, node.auxiliary_size));
  node.pending_addition = add_exact(node.pending_addition, delta);
}

void LinkCutForest::apply_reverse(Vertex vertex) noexcept {
  if (vertex == kNone) return;
  std::swap(nodes_[vertex].left, nodes_[vertex].right);
  nodes_[vertex].reversed = !nodes_[vertex].reversed;
}

void LinkCutForest::push(Vertex vertex) {
  Node& node = nodes_[vertex];
  if (node.has_assignment) {
    apply_assignment(node.left, node.assignment_value);
    apply_assignment(node.right, node.assignment_value);
    node.has_assignment = false;
  }
  if (!exact_is_zero(node.pending_addition)) {
    const ExactSum delta = node.pending_addition;
    apply_addition(node.left, delta);
    apply_addition(node.right, delta);
    node.pending_addition = ExactSum{};
  }
  if (!node.reversed) return;
  apply_reverse(node.left);
  apply_reverse(node.right);
  node.reversed = false;
}

void LinkCutForest::push_path(Vertex vertex) {
  std::vector<Vertex> path;
  path.push_back(vertex);
  Vertex current = vertex;
  while (!is_auxiliary_root(current)) {
    current = nodes_[current].parent;
    path.push_back(current);
  }
  while (!path.empty()) {
    push(path.back());
    path.pop_back();
  }
}

void LinkCutForest::rotate(Vertex vertex) {
  const Vertex parent = nodes_[vertex].parent;
  const Vertex grand = nodes_[parent].parent;
  const bool vertex_is_right = nodes_[parent].right == vertex;
  if (!is_auxiliary_root(parent)) {
    if (nodes_[grand].left == parent) nodes_[grand].left = vertex;
    else nodes_[grand].right = vertex;
  }
  nodes_[vertex].parent = grand;
  if (vertex_is_right) {
    const Vertex middle = nodes_[vertex].left;
    nodes_[parent].right = middle;
    if (middle != kNone) nodes_[middle].parent = parent;
    nodes_[vertex].left = parent;
  } else {
    const Vertex middle = nodes_[vertex].right;
    nodes_[parent].left = middle;
    if (middle != kNone) nodes_[middle].parent = parent;
    nodes_[vertex].right = parent;
  }
  nodes_[parent].parent = vertex;
  pull(parent);
  pull(vertex);
}

void LinkCutForest::splay(Vertex vertex) {
  push_path(vertex);
  while (!is_auxiliary_root(vertex)) {
    const Vertex parent = nodes_[vertex].parent;
    if (!is_auxiliary_root(parent)) {
      const Vertex grand = nodes_[parent].parent;
      const bool vertex_is_left = nodes_[parent].left == vertex;
      const bool parent_is_left = nodes_[grand].left == parent;
      if (vertex_is_left == parent_is_left) rotate(parent);
      else rotate(vertex);
    }
    rotate(vertex);
  }
}

void LinkCutForest::access(Vertex vertex) {
  Vertex previous = kNone;
  Vertex current = vertex;
  while (current != kNone) {
    splay(current);
    const Vertex old_right = nodes_[current].right;
    if (old_right != kNone) {
      nodes_[current].virtual_size += nodes_[old_right].represented_size;
      nodes_[current].virtual_sum = add_exact(
          nodes_[current].virtual_sum, represented_sum(old_right));
    }
    if (previous != kNone) {
      if (nodes_[current].virtual_size < nodes_[previous].represented_size) {
        throw std::logic_error("link-cut virtual-size invariant violated");
      }
      nodes_[current].virtual_size -= nodes_[previous].represented_size;
      nodes_[current].virtual_sum = subtract_exact(
          nodes_[current].virtual_sum, represented_sum(previous));
    }
    nodes_[current].right = previous;
    if (previous != kNone) nodes_[previous].parent = current;
    pull(current);
    previous = current;
    current = nodes_[current].parent;
  }
  splay(vertex);
}

void LinkCutForest::make_root(Vertex vertex) {
  access(vertex);
  apply_reverse(vertex);
}

Vertex LinkCutForest::find_root(Vertex vertex) {
  access(vertex);
  Vertex current = vertex;
  push(current);
  while (nodes_[current].left != kNone) {
    current = nodes_[current].left;
    push(current);
  }
  splay(current);
  return current;
}

void LinkCutForest::link(Vertex first, Vertex second) {
  validate_vertex(first);
  validate_vertex(second);
  if (first == second) {
    throw std::invalid_argument("link-cut forest cannot link a vertex to itself");
  }
  make_root(first);
  if (find_root(second) == first) {
    throw std::invalid_argument("link would create a represented-tree cycle");
  }
  access(second);
  nodes_[first].parent = second;
  nodes_[second].virtual_size += nodes_[first].represented_size;
  nodes_[second].virtual_sum = add_exact(
      nodes_[second].virtual_sum, represented_sum(first));
  pull(second);
}

void LinkCutForest::cut(Vertex first, Vertex second) {
  validate_vertex(first);
  validate_vertex(second);
  if (first == second) {
    throw std::invalid_argument("cut endpoints must be distinct");
  }
  make_root(first);
  access(second);
  push(second);
  if (nodes_[second].left != first) {
    throw std::invalid_argument("cut endpoints are not a represented-tree edge");
  }
  push(first);
  if (nodes_[first].right != kNone) {
    throw std::invalid_argument("cut endpoints are not a represented-tree edge");
  }
  nodes_[second].left = kNone;
  nodes_[first].parent = kNone;
  pull(second);
}

bool LinkCutForest::connected(Vertex first, Vertex second) {
  validate_vertex(first);
  validate_vertex(second);
  if (first == second) return true;
  return find_root(first) == find_root(second);
}

std::size_t LinkCutForest::path_edge_distance(Vertex first, Vertex second) {
  validate_vertex(first);
  validate_vertex(second);
  if (first == second) return 0;
  make_root(first);
  if (find_root(second) != first) {
    throw std::invalid_argument("path query requires connected vertices");
  }
  access(second);
  return nodes_[second].auxiliary_size - 1;
}

std::size_t LinkCutForest::rooted_subtree_vertex_count(Vertex root,
                                                        Vertex vertex) {
  validate_vertex(root);
  validate_vertex(vertex);
  make_root(root);
  if (find_root(vertex) != root) {
    throw std::invalid_argument(
        "rooted subtree query requires connected vertices");
  }
  access(vertex);
  const std::size_t ancestor_contribution =
      child_represented_size(nodes_[vertex].left);
  if (nodes_[vertex].represented_size < ancestor_contribution) {
    throw std::logic_error("link-cut represented-size invariant violated");
  }
  return nodes_[vertex].represented_size - ancestor_contribution;
}

std::int64_t LinkCutForest::rooted_subtree_sum(Vertex root, Vertex vertex) {
  validate_vertex(root);
  validate_vertex(vertex);
  make_root(root);
  if (find_root(vertex) != root) {
    throw std::invalid_argument("rooted subtree sum requires connected vertices");
  }
  access(vertex);
  const ExactSum subtree = subtract_exact(
      represented_sum(vertex), child_represented_sum(nodes_[vertex].left));
  return narrow_exact(subtree);
}

void LinkCutForest::assign_value(Vertex vertex, std::int64_t value) {
  validate_vertex(vertex);
  access(vertex);
  nodes_[vertex].value = value;
  nodes_[vertex].has_assignment = false;
  nodes_[vertex].pending_addition = ExactSum{};
  pull(vertex);
}

void LinkCutForest::assign_path_value(Vertex first, Vertex second,
                                      std::int64_t value) {
  validate_vertex(first);
  validate_vertex(second);
  make_root(first);
  if (find_root(second) != first) {
    throw std::invalid_argument("path assignment requires connected vertices");
  }
  access(second);
  apply_assignment(second, value);
}

void LinkCutForest::add_path_value(Vertex first, Vertex second,
                                   std::int64_t delta) {
  validate_vertex(first);
  validate_vertex(second);
  make_root(first);
  if (find_root(second) != first) {
    throw std::invalid_argument("path addition requires connected vertices");
  }
  access(second);
  const Node& path = nodes_[second];
  if (!can_add_int64(path.auxiliary_min, delta) ||
      !can_add_int64(path.auxiliary_max, delta)) {
    throw std::overflow_error(
        "link-cut path addition would overflow an individual node value");
  }
  apply_addition(second, exact_from_value(delta));
}

std::int64_t LinkCutForest::path_sum(Vertex first, Vertex second) {
  validate_vertex(first);
  validate_vertex(second);
  if (first == second) {
    access(first);
    return nodes_[first].value;
  }
  make_root(first);
  if (find_root(second) != first) {
    throw std::invalid_argument("path sum requires connected vertices");
  }
  access(second);
  return narrow_exact(nodes_[second].auxiliary_sum);
}

bool LinkCutForest::valid_auxiliary_invariants() const {
  const std::size_t vertex_count = nodes_.size();
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    const Node& node = nodes_[vertex];
    if (node.auxiliary_size == 0 || node.represented_size == 0 ||
        node.auxiliary_min > node.auxiliary_max) return false;
    if (node.has_assignment && !exact_is_zero(node.pending_addition)) return false;
    if (node.parent != kNone && node.parent >= vertex_count) return false;
    if (node.left != kNone) {
      if (node.left >= vertex_count || node.left == vertex ||
          nodes_[node.left].parent != vertex) return false;
    }
    if (node.right != kNone) {
      if (node.right >= vertex_count || node.right == vertex ||
          nodes_[node.right].parent != vertex) return false;
    }
    if (node.left != kNone && node.left == node.right) return false;
  }

  std::vector<unsigned char> parent_state(vertex_count, 0);
  for (Vertex start = 0; start < vertex_count; ++start) {
    if (parent_state[start] != 0) continue;
    std::vector<Vertex> chain;
    Vertex current = start;
    while (current != kNone && parent_state[current] == 0) {
      parent_state[current] = 1;
      chain.push_back(current);
      current = nodes_[current].parent;
    }
    if (current != kNone && parent_state[current] == 1) return false;
    for (const Vertex vertex : chain) parent_state[vertex] = 2;
  }

  struct Frame {
    Vertex vertex;
    bool expanded;
    bool inherited_assignment;
    std::int64_t inherited_value;
    ExactSum inherited_addition;
  };

  std::vector<unsigned char> state(vertex_count, 0);
  std::vector<std::size_t> computed_size(vertex_count, 0);
  std::vector<std::size_t> computed_represented_size(vertex_count, 0);
  std::vector<ExactSum> computed_sum(vertex_count);
  std::vector<ExactSum> computed_auxiliary_virtual_sum(vertex_count);
  std::vector<ExactSum> computed_represented_sum(vertex_count);
  std::vector<std::int64_t> computed_min(vertex_count, 0);
  std::vector<std::int64_t> computed_max(vertex_count, 0);

  for (Vertex root = 0; root < vertex_count; ++root) {
    if (!is_auxiliary_root(root) || state[root] != 0) continue;
    std::vector<Frame> stack;
    stack.push_back(Frame{root, false, false, 0, ExactSum{}});
    while (!stack.empty()) {
      const Frame frame = stack.back();
      stack.pop_back();
      const Vertex vertex = frame.vertex;
      const Node& node = nodes_[vertex];
      if (!frame.expanded) {
        if (state[vertex] == 1 || state[vertex] == 2) return false;
        state[vertex] = 1;
        bool child_assignment = false;
        std::int64_t child_assignment_value = 0;
        ExactSum child_addition{};
        if (frame.inherited_assignment) {
          child_assignment = true;
          child_assignment_value = frame.inherited_value;
        } else if (node.has_assignment) {
          const ExactSum assigned = add_exact(
              exact_from_value(node.assignment_value), frame.inherited_addition);
          if (!exact_fits_int64(assigned)) return false;
          child_assignment = true;
          child_assignment_value = narrow_exact_unchecked(assigned);
        } else {
          child_addition = add_exact(node.pending_addition,
                                     frame.inherited_addition);
        }
        stack.push_back(Frame{vertex, true, frame.inherited_assignment,
                              frame.inherited_value,
                              frame.inherited_addition});
        if (node.right != kNone) {
          stack.push_back(Frame{node.right, false, child_assignment,
                                child_assignment_value, child_addition});
        }
        if (node.left != kNone) {
          stack.push_back(Frame{node.left, false, child_assignment,
                                child_assignment_value, child_addition});
        }
        continue;
      }

      const std::size_t expected_size =
          1 + (node.left == kNone ? 0 : computed_size[node.left]) +
          (node.right == kNone ? 0 : computed_size[node.right]);
      if (node.auxiliary_size != expected_size) return false;

      const std::size_t expected_represented_size =
          1 + node.virtual_size +
          (node.left == kNone ? 0 : computed_represented_size[node.left]) +
          (node.right == kNone ? 0 : computed_represented_size[node.right]);
      if (node.represented_size != expected_represented_size) return false;

      const ExactSum expected_auxiliary_virtual_sum = add_exact(
          node.virtual_sum,
          add_exact(node.left == kNone
                        ? ExactSum{}
                        : computed_auxiliary_virtual_sum[node.left],
                    node.right == kNone
                        ? ExactSum{}
                        : computed_auxiliary_virtual_sum[node.right]));
      if (!exact_equal(node.auxiliary_virtual_sum,
                       expected_auxiliary_virtual_sum)) {
        return false;
      }

      std::int64_t own_value = 0;
      if (frame.inherited_assignment) {
        own_value = frame.inherited_value;
      } else {
        const ExactSum shifted = add_exact(exact_from_value(node.value),
                                           frame.inherited_addition);
        if (!exact_fits_int64(shifted)) return false;
        own_value = narrow_exact_unchecked(shifted);
      }

      ExactSum expected_sum = exact_from_value(own_value);
      std::int64_t expected_min = own_value;
      std::int64_t expected_max = own_value;
      if (node.left != kNone) {
        expected_sum = add_exact(computed_sum[node.left], expected_sum);
        expected_min = std::min(expected_min, computed_min[node.left]);
        expected_max = std::max(expected_max, computed_max[node.left]);
      }
      if (node.right != kNone) {
        expected_sum = add_exact(expected_sum, computed_sum[node.right]);
        expected_min = std::min(expected_min, computed_min[node.right]);
        expected_max = std::max(expected_max, computed_max[node.right]);
      }

      const bool inherited_identity =
          !frame.inherited_assignment && exact_is_zero(frame.inherited_addition);
      if (inherited_identity) {
        if (!exact_equal(node.auxiliary_sum, expected_sum) ||
            node.auxiliary_min != expected_min ||
            node.auxiliary_max != expected_max) return false;
        if (node.has_assignment && node.value != node.assignment_value) return false;
      }

      computed_size[vertex] = expected_size;
      computed_represented_size[vertex] = expected_represented_size;
      computed_sum[vertex] = expected_sum;
      computed_auxiliary_virtual_sum[vertex] = expected_auxiliary_virtual_sum;
      computed_represented_sum[vertex] =
          add_exact(expected_sum, expected_auxiliary_virtual_sum);
      computed_min[vertex] = expected_min;
      computed_max[vertex] = expected_max;
      state[vertex] = 2;
    }
  }

  for (const unsigned char value : state) if (value != 2) return false;

  std::vector<std::size_t> direct_virtual_size(vertex_count, 0);
  std::vector<ExactSum> direct_virtual_sum(vertex_count);
  for (Vertex child = 0; child < vertex_count; ++child) {
    const Vertex parent = nodes_[child].parent;
    if (parent == kNone || nodes_[parent].left == child ||
        nodes_[parent].right == child) {
      continue;
    }
    direct_virtual_size[parent] += computed_represented_size[child];
    direct_virtual_sum[parent] = add_exact(
        direct_virtual_sum[parent], computed_represented_sum[child]);
  }
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (nodes_[vertex].virtual_size != direct_virtual_size[vertex] ||
        !exact_equal(nodes_[vertex].virtual_sum, direct_virtual_sum[vertex])) {
      return false;
    }
  }
  return true;
}

}  // namespace algorithms::graphs
