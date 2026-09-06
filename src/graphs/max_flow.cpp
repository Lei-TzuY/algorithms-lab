#include "algorithms/graphs/max_flow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

struct ResidualArc {
  Vertex to;
  std::size_t reverse_index;
  Capacity residual;
};

struct ArcLocation {
  Vertex from;
  std::size_t index;
  bool materialized;
};

[[nodiscard]] Capacity checked_add(Capacity left, Capacity right) {
  if (right > 0 && left > std::numeric_limits<Capacity>::max() - right) {
    throw std::overflow_error("max-flow capacity sum is not representable");
  }
  return left + right;
}

class DinicSolver {
 public:
  explicit DinicSolver(std::size_t vertex_count)
      : adjacency_(vertex_count), level_(vertex_count), next_arc_(vertex_count) {}

  [[nodiscard]] ArcLocation add_edge(Vertex from, Vertex to, Capacity capacity) {
    if (from == to) {
      return ArcLocation{from, 0, false};
    }

    const std::size_t forward_index = adjacency_[from].size();
    const std::size_t reverse_index = adjacency_[to].size();
    adjacency_[from].push_back(ResidualArc{to, reverse_index, capacity});
    adjacency_[to].push_back(ResidualArc{from, forward_index, 0});
    return ArcLocation{from, forward_index, true};
  }

  [[nodiscard]] Capacity solve(Vertex source, Vertex sink) {
    Capacity total = 0;
    while (build_levels(source, sink)) {
      std::fill(next_arc_.begin(), next_arc_.end(), std::size_t{0});
      while (true) {
        const Capacity pushed =
            send_flow(source, sink, std::numeric_limits<Capacity>::max());
        if (pushed == 0) {
          break;
        }
        total = checked_add(total, pushed);
      }
    }
    return total;
  }

  [[nodiscard]] Capacity residual_at(const ArcLocation& location) const {
    return adjacency_[location.from][location.index].residual;
  }

  [[nodiscard]] std::vector<bool> residual_reachable(Vertex source) const {
    std::vector<bool> seen(adjacency_.size(), false);
    std::queue<Vertex> queue;
    seen[source] = true;
    queue.push(source);
    while (!queue.empty()) {
      const Vertex vertex = queue.front();
      queue.pop();
      for (const auto& arc : adjacency_[vertex]) {
        if (arc.residual > 0 && !seen[arc.to]) {
          seen[arc.to] = true;
          queue.push(arc.to);
        }
      }
    }
    return seen;
  }

 private:
  [[nodiscard]] bool build_levels(Vertex source, Vertex sink) {
    std::fill(level_.begin(), level_.end(), -1);
    std::queue<Vertex> queue;
    level_[source] = 0;
    queue.push(source);
    while (!queue.empty()) {
      const Vertex vertex = queue.front();
      queue.pop();
      for (const auto& arc : adjacency_[vertex]) {
        if (arc.residual > 0 && level_[arc.to] < 0) {
          level_[arc.to] = level_[vertex] + 1;
          queue.push(arc.to);
        }
      }
    }
    return level_[sink] >= 0;
  }

  [[nodiscard]] Capacity send_flow(Vertex vertex, Vertex sink, Capacity limit) {
    if (vertex == sink) {
      return limit;
    }

    auto& cursor = next_arc_[vertex];
    while (cursor < adjacency_[vertex].size()) {
      auto& arc = adjacency_[vertex][cursor];
      if (arc.residual > 0 && level_[arc.to] == level_[vertex] + 1) {
        const Capacity pushed =
            send_flow(arc.to, sink, std::min(limit, arc.residual));
        if (pushed > 0) {
          arc.residual -= pushed;
          auto& reverse = adjacency_[arc.to][arc.reverse_index];
          reverse.residual = checked_add(reverse.residual, pushed);
          return pushed;
        }
      }
      ++cursor;
    }
    return 0;
  }

  std::vector<std::vector<ResidualArc>> adjacency_;
  std::vector<std::int64_t> level_;
  std::vector<std::size_t> next_arc_;
};

void validate_input(std::size_t vertex_count,
                    std::span<const CapacityEdge> edges, Vertex source,
                    Vertex sink) {
  if (source >= vertex_count || sink >= vertex_count) {
    throw std::out_of_range("max-flow source or sink is out of range");
  }
  if (source == sink) {
    throw std::invalid_argument("max-flow source and sink must differ");
  }
  for (const auto& edge : edges) {
    if (edge.from >= vertex_count || edge.to >= vertex_count) {
      throw std::out_of_range("max-flow edge endpoint is out of range");
    }
    if (edge.capacity < 0) {
      throw std::invalid_argument("max-flow capacities must be non-negative");
    }
  }
}

}  // namespace

MaxFlowResult dinic_max_flow(std::size_t vertex_count,
                             std::span<const CapacityEdge> edges,
                             Vertex source, Vertex sink) {
  validate_input(vertex_count, edges, source, sink);

  DinicSolver solver(vertex_count);
  std::vector<ArcLocation> locations;
  locations.reserve(edges.size());
  for (const auto& edge : edges) {
    locations.push_back(solver.add_edge(edge.from, edge.to, edge.capacity));
  }

  const Capacity value = solver.solve(source, sink);
  auto source_side = solver.residual_reachable(source);
  if (source_side[sink]) {
    throw std::logic_error("Dinic terminated with a residual s-t path");
  }

  std::vector<FlowEdge> flows;
  flows.reserve(edges.size());
  Capacity cut_capacity = 0;
  for (std::size_t index = 0; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const auto& location = locations[index];
    const Capacity flow = location.materialized
                              ? edge.capacity - solver.residual_at(location)
                              : 0;
    flows.push_back(FlowEdge{edge.from, edge.to, edge.capacity, flow});
    if (source_side[edge.from] && !source_side[edge.to]) {
      cut_capacity = checked_add(cut_capacity, edge.capacity);
    }
  }

  if (cut_capacity != value) {
    throw std::logic_error("max-flow/min-cut certificate mismatch");
  }
  return MaxFlowResult{value, cut_capacity, std::move(flows),
                       std::move(source_side)};
}

}  // namespace algorithms::graphs
