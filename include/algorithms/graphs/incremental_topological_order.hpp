#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace algorithms::graphs {

class IncrementalTopologicalOrder {
 public:
  explicit IncrementalTopologicalOrder(std::size_t vertex_count);

  [[nodiscard]] std::size_t vertex_count() const noexcept;
  [[nodiscard]] std::size_t edge_count() const noexcept;
  [[nodiscard]] std::span<const Vertex> order() const noexcept;
  [[nodiscard]] std::size_t position(Vertex vertex) const;

  // Inserts one directed edge while preserving a valid topological order.
  // Parallel edges are accepted. A self-loop or any edge that would create a
  // directed cycle is rejected transactionally and returns false.
  [[nodiscard]] bool try_add_edge(Vertex from, Vertex to);

 private:
  Graph graph_;
  std::size_t edge_count_{};
  std::vector<Vertex> order_;
  std::vector<std::size_t> position_;
};

}  // namespace algorithms::graphs
