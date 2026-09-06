#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::graphs {

using Vertex = std::size_t;
using Weight = std::int64_t;

struct Edge {
  Vertex to;
  Weight weight;

  friend bool operator==(const Edge&, const Edge&) = default;
};

class Graph {
 public:
  explicit Graph(std::size_t vertex_count, bool directed);

  [[nodiscard]] std::size_t vertex_count() const noexcept;
  [[nodiscard]] bool directed() const noexcept;

  // Self-loops and parallel edges are supported intentionally.
  // Iteration order is deterministic and matches insertion order.
  void add_edge(Vertex from, Vertex to, Weight weight = 1);

  [[nodiscard]] const std::vector<Edge>& neighbors(Vertex vertex) const;
  [[nodiscard]] const std::vector<std::vector<Edge>>& adjacency() const noexcept;
  void validate_vertex(Vertex vertex) const;

 private:
  bool directed_;
  std::vector<std::vector<Edge>> adjacency_;
};

}  // namespace algorithms::graphs
