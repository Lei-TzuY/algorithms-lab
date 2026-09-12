#pragma once

#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

// Enumerates every distinct simple directed cycle under structural vertex-cycle
// semantics. Parallel arcs and weights do not duplicate a cycle. A self-loop is
// represented by the one-vertex cycle {v}. Each returned cycle starts at its
// smallest vertex, and the complete result is lexicographically sorted.
//
// The implementation follows Johnson's SCC-restricted blocked-set algorithm.
// After deterministic adjacency canonicalization, the classical output-sensitive
// core bound is O((V + E)(C + 1)) after O(M log M) adjacency canonicalization,
// where M is the number of input arc copies and C is the number of cycles. The
// final deterministic lexicographic sort adds O(C log C * V) worst-case comparison
// work. Auxiliary state is O(V + E) excluding output and recursion stack.
[[nodiscard]] std::vector<std::vector<Vertex>> johnson_elementary_cycles(
    const Graph& graph);

}  // namespace algorithms::graphs
