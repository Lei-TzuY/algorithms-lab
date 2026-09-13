#pragma once

#include "algorithms/graphs/open_ear_decomposition.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

enum class STNumberingStatus {
  success,
  not_two_vertex_connected,
};

struct STNumberingResult {
  STNumberingStatus status = STNumberingStatus::not_two_vertex_connected;
  std::vector<Vertex> order;
  std::vector<std::size_t> rank;
  std::optional<Vertex> blocking_articulation;

  friend bool operator==(const STNumberingResult&, const STNumberingResult&) =
      default;
};

// Construct a deterministic st-numbering of a simple, undirected,
// two-vertex-connected graph. Edge weights are ignored.
//
// The source/sink are the endpoints of the closing edge of the initial cycle
// returned by open_ear_decomposition(). The result order places source first and
// sink last; every other vertex has at least one lower-numbered and at least one
// higher-numbered neighbor.
//
// Directed input, self-loops, and parallel edges inherit the open-ear domain
// rejection. Non-two-connected input returns not_two_vertex_connected and
// propagates the articulation witness when one is available.
//
// Conversion of the ear witness uses O(V^2) time in this direct vector-insertion
// implementation and O(V) additional storage. Including the reused open-ear
// construction, the conservative end-to-end bound is O(V(V + E)) time and
// O(V + E) storage.
[[nodiscard]] inline STNumberingResult st_numbering(const Graph& graph) {
  const OpenEarDecompositionResult decomposition = open_ear_decomposition(graph);

  STNumberingResult result;
  result.blocking_articulation = decomposition.blocking_articulation;
  if (decomposition.status != OpenEarStatus::success) {
    return result;
  }
  if (decomposition.ears.empty()) {
    throw std::logic_error("successful open-ear decomposition had no ears");
  }

  const std::vector<Vertex>& cycle = decomposition.ears.front().vertices;
  if (cycle.size() < 4 || cycle.front() != cycle.back()) {
    throw std::logic_error("initial open ear was not a simple cycle");
  }

  const std::size_t vertex_count = graph.vertex_count();
  const std::size_t unnumbered = std::numeric_limits<std::size_t>::max();
  result.order.assign(cycle.begin(), cycle.end() - 1);
  result.rank.assign(vertex_count, unnumbered);

  const auto rebuild_rank = [&]() {
    std::fill(result.rank.begin(), result.rank.end(), unnumbered);
    for (std::size_t index = 0; index < result.order.size(); ++index) {
      const Vertex vertex = result.order[index];
      if (vertex >= vertex_count || result.rank[vertex] != unnumbered) {
        throw std::logic_error("open-ear witness repeated an incorporated vertex");
      }
      result.rank[vertex] = index;
    }
  };
  rebuild_rank();

  for (std::size_t ear_index = 1; ear_index < decomposition.ears.size();
       ++ear_index) {
    const std::vector<Vertex>& path = decomposition.ears[ear_index].vertices;
    if (path.size() < 2) {
      throw std::logic_error("open-ear witness contained a short ear");
    }
    if (path.size() == 2) {
      continue;
    }

    const Vertex first = path.front();
    const Vertex last = path.back();
    if (first >= vertex_count || last >= vertex_count ||
        result.rank[first] == unnumbered || result.rank[last] == unnumbered) {
      throw std::logic_error("open-ear endpoints were not already numbered");
    }

    std::vector<Vertex> internal(path.begin() + 1, path.end() - 1);
    for (Vertex vertex : internal) {
      if (vertex >= vertex_count || result.rank[vertex] != unnumbered) {
        throw std::logic_error("open-ear internal vertex was already numbered");
      }
    }

    const std::size_t first_rank = result.rank[first];
    const std::size_t last_rank = result.rank[last];
    if (first_rank > last_rank) {
      std::reverse(internal.begin(), internal.end());
    }
    const std::size_t insertion_index = std::min(first_rank, last_rank) + 1;
    result.order.insert(
        result.order.begin() + static_cast<std::ptrdiff_t>(insertion_index),
        internal.begin(), internal.end());
    rebuild_rank();
  }

  if (result.order.size() != vertex_count ||
      std::find(result.rank.begin(), result.rank.end(), unnumbered) !=
          result.rank.end()) {
    throw std::logic_error("open-ear witness did not incorporate every vertex");
  }

  result.status = STNumberingStatus::success;
  return result;
}

}  // namespace algorithms::graphs
