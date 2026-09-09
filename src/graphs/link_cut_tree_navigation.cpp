#include "algorithms/graphs/link_cut_tree.hpp"

#include <stdexcept>

namespace algorithms::graphs {

Vertex LinkCutForest::kth_vertex_on_path(Vertex first, Vertex second,
                                         std::size_t rank) {
  validate_vertex(first);
  validate_vertex(second);

  make_root(first);
  if (find_root(second) != first) {
    throw std::invalid_argument(
        "ordered path navigation requires connected vertices");
  }
  access(second);

  if (rank >= nodes_[second].auxiliary_size) {
    throw std::out_of_range("ordered path rank is outside represented path");
  }

  Vertex current = second;
  for (;;) {
    push(current);
    const Vertex left = nodes_[current].left;
    const std::size_t left_size = child_size(left);

    if (rank < left_size) {
      current = left;
      continue;
    }
    if (rank == left_size) {
      splay(current);
      return current;
    }

    rank -= left_size + 1;
    current = nodes_[current].right;
  }
}

}  // namespace algorithms::graphs
