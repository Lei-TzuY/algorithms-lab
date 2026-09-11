#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct PerfectEliminationViolation {
  Vertex vertex = 0;
  Vertex first_later_neighbor = 0;
  Vertex second_later_neighbor = 0;

  friend bool operator==(const PerfectEliminationViolation&,
                         const PerfectEliminationViolation&) = default;
};

struct ChordalRecognitionResult {
  bool chordal = false;
  std::vector<Vertex> mcs_selection_order;
  std::vector<Vertex> candidate_elimination_order;
  std::optional<PerfectEliminationViolation> violation;
  std::optional<std::size_t> maximum_clique_size;

  friend bool operator==(const ChordalRecognitionResult&,
                         const ChordalRecognitionResult&) = default;
};

// Recognizes chordality of the underlying simple graph with deterministic
// Maximum Cardinality Search. Self-loops are ignored, parallel copies collapse,
// and edge weights do not participate in chordality.
//
// When chordal=true, candidate_elimination_order is an exact perfect
// elimination ordering and maximum_clique_size is exact. When chordal=false,
// violation identifies two non-adjacent later neighbors in the MCS-derived
// candidate ordering; it is not an induced-cycle certificate by itself.
[[nodiscard]] ChordalRecognitionResult recognize_chordal_graph(const Graph& graph);

}  // namespace algorithms::graphs
