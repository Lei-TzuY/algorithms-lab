#include "algorithms/graphs/link_cut_tree.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

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

void LinkCutForest::pull(Vertex vertex) noexcept {
  nodes_[vertex].auxiliary_size =
      1 + child_size(nodes_[vertex].left) + child_size(nodes_[vertex].right);
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

  // Parent pointers include both auxiliary parents and represented path-parents,
  // but they must still form an acyclic parent-pointer forest.
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
        if (state[vertex] == 1) {
          return false;
        }
        if (state[vertex] == 2) {
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

      const std::size_t expected =
          1 + (nodes_[vertex].left == kNone
                   ? 0
                   : computed_size[nodes_[vertex].left]) +
          (nodes_[vertex].right == kNone
               ? 0
               : computed_size[nodes_[vertex].right]);
      if (nodes_[vertex].auxiliary_size != expected) {
        return false;
      }
      computed_size[vertex] = expected;
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
