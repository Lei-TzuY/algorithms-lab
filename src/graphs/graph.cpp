#include "algorithms/graphs/graph.hpp"

#include <stdexcept>

namespace algorithms::graphs {

Graph::Graph(std::size_t vertex_count, bool directed)
    : directed_(directed), adjacency_(vertex_count) {}

std::size_t Graph::vertex_count() const noexcept { return adjacency_.size(); }

bool Graph::directed() const noexcept { return directed_; }

void Graph::add_edge(Vertex from, Vertex to, Weight weight) {
  validate_vertex(from);
  validate_vertex(to);
  adjacency_[from].push_back(Edge{to, weight});
  if (!directed_ && from != to) {
    adjacency_[to].push_back(Edge{from, weight});
  }
}

const std::vector<Edge>& Graph::neighbors(Vertex vertex) const {
  validate_vertex(vertex);
  return adjacency_[vertex];
}

const std::vector<std::vector<Edge>>& Graph::adjacency() const noexcept {
  return adjacency_;
}

void Graph::validate_vertex(Vertex vertex) const {
  if (vertex >= adjacency_.size()) {
    throw std::out_of_range("graph vertex out of range");
  }
}

}  // namespace algorithms::graphs
