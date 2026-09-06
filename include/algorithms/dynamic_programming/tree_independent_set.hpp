#pragma once

#include <cstdint>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::dynamic_programming {

struct TreeIndependentSetResult {
  std::int64_t maximum_weight{0};
  std::vector<graphs::Vertex> vertices;
};

// Computes a maximum-weight independent set on an undirected tree. Edge
// weights are ignored; vertex_weights supplies the objective value for each
// dense vertex ID. The empty set is allowed, so an all-negative tree has
// optimum zero. The root only orients the tree for DP and does not change the
// optimum value. Equal take/skip states deterministically choose skip.
//
// Throws std::invalid_argument if the graph is directed, disconnected,
// cyclic, contains a self-loop/parallel edge, or the weight count mismatches.
// Throws std::overflow_error if a feasible state weight is not representable
// in int64_t.
TreeIndependentSetResult maximum_weight_independent_set_tree(
    const graphs::Graph& tree,
    const std::vector<std::int64_t>& vertex_weights,
    graphs::Vertex root = 0);

}  // namespace algorithms::dynamic_programming
