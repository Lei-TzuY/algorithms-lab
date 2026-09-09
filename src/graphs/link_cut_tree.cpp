#include "algorithms/graphs/link_cut_tree.hpp"

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
  if (parent == kNone) {
    return true;
  }
  return nodes_[parent].left != vertex && nodes_[parent].right != vertex;
}

std::size_t LinkCutForest::child_size(Vertex vertex) const noexcept {
  return vertex == kNone ? 0 : nodes_[vertex].auxiliary_size;
}

LinkCutForest::ExactSum LinkCutForest::child_sum(Vertex vertex) const noexcept {
  return vertex == kNone ? ExactSum{} : nodes_[vertex].auxiliary_sum;
}

LinkCutForest::ExactSum LinkCutForest::exact_from_value(
    std::int64_t value) noexcept {
  if (value >= 0) {
    return ExactSum{static_cast<std::uint64_t>(value), 0, false};
  }
  std::uint64_t magnitude = static_cast<std::uint64_t>(-(value + 1));
  ++magnitude;
  return ExactSum{magnitude, 0, true};
}

int LinkCutForest::compare_magnitude(const ExactSum& first,
                                     const ExactSum& second) noexcept {
  if (first.high != second.high) {
    return first.high < second.high ? -1 : 1;
  }
  if (first.low != second.low) {
    return first.low < second.low ? -1 : 1;
  }
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
  if (comparison == 0) {
    return ExactSum{};
  }
  if (comparison > 0) {
    ExactSum result = subtract_magnitude(first, second);
    result.negative = first.negative;
    return result;
  }
  ExactSum result = subtract_magnitude(second, first);
  result.negative = second.negative;
  return result;
}

bool LinkCutForest::exact_equal(const ExactSum& first,
                                const ExactSum& second) noexcept {
  return first.low == second.low && first.high == second.high &&
         first.negative == second.negative;
}

std::int64_t LinkCutForest::narrow_exact(const ExactSum& value) {
  if (value.high != 0) {
    throw std::overflow_error("link-cut path sum is outside int64");
  }
  constexpr std::uint64_t kNegativeLimit = std::uint64_t{1} << 63U;
  constexpr std::uint64_t kPositiveLimit = kNegativeLimit - 1U;
  if (!value.negative) {
    if (value.low > kPositiveLimit) {
      throw std::overflow_error("link-cut path sum is outside int64");
    }
    return static_cast<std::int64_t>(value.low);
  }
  if (value.low > kNegativeLimit) {
    throw std::overflow_error("link-cut path sum is outside int64");
  }
  if (value.low == kNegativeLimit) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return -static_cast<std::int64_t>(value.low);
}

void LinkCutForest::pull(Vertex vertex) noexcept {
  nodes_[vertex].auxiliary_size =
      1 + child_size(nodes_[vertex].left) + child_size(nodes_[vertex].right);
  nodes_[vertex].auxiliary_sum =
      add_exact(add_exact(child_sum(nodes_[vertex].left),
                          exact_from_value(nodes_[vertex].value)),
                child_sum(nodes_[vertex].right));
}

void LinkCutForest::apply_reverse(Vertex vertex) noexcept {
  if (vertex == kNone) {
    return;
  }
  std::swap(nodes_[vertex].left, nodes_[vertex].right);
  nodes_[vertex].reversed = !nodes_[vertex].reversed;
}

void LinkCutForest::push(Vertex vertex) noexcept {
  if (!nodes_[vertex].reversed) {
    return;
  }
  apply_reverse(nodes_[vertex].left);
  apply_reverse(nodes_[vertex].right);
  nodes_[vertex].reversed = false;
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

void LinkCutForest::rotate(Vertex vertex) noexcept {
  const Vertex parent = nodes_[vertex].parent;
  const Vertex grand = nodes_[parent].parent;
  const bool vertex_is_right = nodes_[parent].right == vertex;

  if (!is_auxiliary_root(parent)) {
    if (nodes_[grand].left == parent) {
      nodes_[grand].left = vertex;
    } else {
      nodes_[grand].right = vertex;
    }
  }
  nodes_[vertex].parent = grand;

  if (vertex_is_right) {
    const Vertex middle = nodes_[vertex].left;
    nodes_[parent].right = middle;
    if (middle != kNone) {
      nodes_[middle].parent = parent;
    }
    nodes_[vertex].left = parent;
  } else {
    const Vertex middle = nodes_[vertex].right;
    nodes_[parent].left = middle;
    if (middle != kNone) {
      nodes_[middle].parent = parent;
    }
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
      if (vertex_is_left == parent_is_left) {
        rotate(parent);
      } else {
        rotate(vertex);
      }
    }
    rotate(vertex);
  }
}

void LinkCutForest::access(Vertex vertex) {
  Vertex previous = kNone;
  Vertex current = vertex;
  while (current != kNone) {
    splay(current);
    nodes_[current].right = previous;
    if (previous != kNone) {
      nodes_[previous].parent = current;
    }
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
  nodes_[first].parent = second;
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
  if (first == second) {
    return true;
  }
  return find_root(first) == find_root(second);
}

std::size_t LinkCutForest::path_edge_distance(Vertex first, Vertex second) {
  validate_vertex(first);
  validate_vertex(second);
  if (first == second) {
    return 0;
  }

  make_root(first);
  if (find_root(second) != first) {
    throw std::invalid_argument("path query requires connected vertices");
  }
  access(second);
  return nodes_[second].auxiliary_size - 1;
}

void LinkCutForest::assign_value(Vertex vertex, std::int64_t value) {
  validate_vertex(vertex);
  access(vertex);
  nodes_[vertex].value = value;
  pull(vertex);
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
    if (node.auxiliary_size == 0) {
      return false;
    }
    if (node.parent != kNone && node.parent >= vertex_count) {
      return false;
    }
    if (node.left != kNone) {
      if (node.left >= vertex_count || node.left == vertex ||
          nodes_[node.left].parent != vertex) {
        return false;
      }
    }
    if (node.right != kNone) {
      if (node.right >= vertex_count || node.right == vertex ||
          nodes_[node.right].parent != vertex) {
        return false;
      }
    }
    if (node.left != kNone && node.left == node.right) {
      return false;
    }
  }

  std::vector<unsigned char> parent_state(vertex_count, 0);
  for (Vertex start = 0; start < vertex_count; ++start) {
    if (parent_state[start] != 0) {
      continue;
    }
    std::vector<Vertex> chain;
    Vertex current = start;
    while (current != kNone && parent_state[current] == 0) {
      parent_state[current] = 1;
      chain.push_back(current);
      current = nodes_[current].parent;
    }
    if (current != kNone && parent_state[current] == 1) {
      return false;
    }
    for (const Vertex vertex : chain) {
      parent_state[vertex] = 2;
    }
  }

  std::vector<unsigned char> state(vertex_count, 0);
  std::vector<std::size_t> computed_size(vertex_count, 0);
  std::vector<ExactSum> computed_sum(vertex_count);
  for (Vertex root = 0; root < vertex_count; ++root) {
    if (!is_auxiliary_root(root) || state[root] != 0) {
      continue;
    }

    struct Frame {
      Vertex vertex;
      bool expanded;
    };
    std::vector<Frame> stack;
    stack.push_back(Frame{root, false});
    while (!stack.empty()) {
      const Frame frame = stack.back();
      stack.pop_back();
      const Vertex vertex = frame.vertex;
      if (!frame.expanded) {
        if (state[vertex] == 1 || state[vertex] == 2) {
          return false;
        }
        state[vertex] = 1;
        stack.push_back(Frame{vertex, true});
        if (nodes_[vertex].right != kNone) {
          stack.push_back(Frame{nodes_[vertex].right, false});
        }
        if (nodes_[vertex].left != kNone) {
          stack.push_back(Frame{nodes_[vertex].left, false});
        }
        continue;
      }

      const std::size_t expected_size =
          1 + (nodes_[vertex].left == kNone
                   ? 0
                   : computed_size[nodes_[vertex].left]) +
          (nodes_[vertex].right == kNone
               ? 0
               : computed_size[nodes_[vertex].right]);
      if (nodes_[vertex].auxiliary_size != expected_size) {
        return false;
      }

      const ExactSum expected_sum = add_exact(
          add_exact(nodes_[vertex].left == kNone
                        ? ExactSum{}
                        : computed_sum[nodes_[vertex].left],
                    exact_from_value(nodes_[vertex].value)),
          nodes_[vertex].right == kNone
              ? ExactSum{}
              : computed_sum[nodes_[vertex].right]);
      if (!exact_equal(nodes_[vertex].auxiliary_sum, expected_sum)) {
        return false;
      }

      computed_size[vertex] = expected_size;
      computed_sum[vertex] = expected_sum;
      state[vertex] = 2;
    }
  }

  for (const unsigned char value : state) {
    if (value != 2) {
      return false;
    }
  }
  return true;
}

}  // namespace algorithms::graphs
