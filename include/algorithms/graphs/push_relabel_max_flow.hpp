#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/max_flow.hpp"

namespace algorithms::graphs {
namespace push_relabel_detail {

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

[[nodiscard]] inline Capacity checked_capacity_add(Capacity left,
                                                   Capacity right) {
  if (right > 0 && left > std::numeric_limits<Capacity>::max() - right) {
    throw std::overflow_error("push-relabel capacity sum is not representable");
  }
  return left + right;
}

// A fixed two-limb accumulator is enough for every realizable input on targets
// where size_t is at most 64 bits: at most SIZE_MAX edges can each contribute at
// most INT64_MAX units, so total preflow is strictly below 2^127.
static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t),
              "push-relabel wide excess assumes size_t is at most 64 bits");

class WideExcess {
 public:
  [[nodiscard]] bool is_zero() const noexcept { return high_ == 0 && low_ == 0; }

  void add(std::uint64_t amount) {
    const std::uint64_t previous = low_;
    low_ += amount;
    if (low_ < previous) {
      if (high_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("push-relabel wide excess is not representable");
      }
      ++high_;
    }
  }

  void subtract(std::uint64_t amount) {
    if (high_ == 0 && low_ < amount) {
      throw std::logic_error("push-relabel excess underflow");
    }
    const std::uint64_t previous = low_;
    low_ -= amount;
    if (previous < amount) {
      --high_;
    }
  }

  [[nodiscard]] std::uint64_t bounded_by(std::uint64_t upper) const noexcept {
    if (high_ != 0) {
      return upper;
    }
    return std::min(low_, upper);
  }

  [[nodiscard]] Capacity to_capacity() const {
    const auto maximum =
        static_cast<std::uint64_t>(std::numeric_limits<Capacity>::max());
    if (high_ != 0 || low_ > maximum) {
      throw std::overflow_error("push-relabel maximum flow is not representable");
    }
    return static_cast<Capacity>(low_);
  }

 private:
  std::uint64_t high_ = 0;
  std::uint64_t low_ = 0;
};

inline void validate_input(std::size_t vertex_count,
                           std::span<const CapacityEdge> edges, Vertex source,
                           Vertex sink) {
  if (source >= vertex_count || sink >= vertex_count) {
    throw std::out_of_range("push-relabel source or sink is out of range");
  }
  if (source == sink) {
    throw std::invalid_argument("push-relabel source and sink must differ");
  }
  for (const auto& edge : edges) {
    if (edge.from >= vertex_count || edge.to >= vertex_count) {
      throw std::out_of_range("push-relabel edge endpoint is out of range");
    }
    if (edge.capacity < 0) {
      throw std::invalid_argument(
          "push-relabel capacities must be non-negative");
    }
  }
}

class Solver {
 public:
  Solver(std::size_t vertex_count, Vertex source, Vertex sink)
      : adjacency_(vertex_count), height_(vertex_count, 0),
        excess_(vertex_count), next_arc_(vertex_count, 0),
        in_queue_(vertex_count, false), source_(source), sink_(sink) {}

  [[nodiscard]] ArcLocation add_edge(Vertex from, Vertex to,
                                     Capacity capacity) {
    if (from == to) {
      return ArcLocation{from, 0, false};
    }
    const std::size_t forward_index = adjacency_[from].size();
    const std::size_t reverse_index = adjacency_[to].size();
    adjacency_[from].push_back(ResidualArc{to, reverse_index, capacity});
    adjacency_[to].push_back(ResidualArc{from, forward_index, 0});
    return ArcLocation{from, forward_index, true};
  }

  void solve() {
    height_[source_] = adjacency_.size();
    for (auto& arc : adjacency_[source_]) {
      if (arc.residual == 0) {
        continue;
      }
      const Capacity pushed = arc.residual;
      arc.residual = 0;
      auto& reverse = adjacency_[arc.to][arc.reverse_index];
      reverse.residual = checked_capacity_add(reverse.residual, pushed);
      if (arc.to != source_) {
        const auto amount = static_cast<std::uint64_t>(pushed);
        excess_[arc.to].add(amount);
        enqueue_if_active(arc.to);
      }
    }

    while (!active_.empty()) {
      const Vertex vertex = active_.front();
      active_.pop();
      in_queue_[vertex] = false;
      discharge(vertex);
    }
  }

  [[nodiscard]] Capacity value() const { return excess_[sink_].to_capacity(); }

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
  void enqueue_if_active(Vertex vertex) {
    if (vertex == source_ || vertex == sink_ || excess_[vertex].is_zero() ||
        in_queue_[vertex]) {
      return;
    }
    in_queue_[vertex] = true;
    active_.push(vertex);
  }

  void push(Vertex from, ResidualArc& arc) {
    const std::uint64_t residual = static_cast<std::uint64_t>(arc.residual);
    const std::uint64_t amount = excess_[from].bounded_by(residual);
    if (amount == 0) {
      return;
    }
    const Capacity pushed = static_cast<Capacity>(amount);
    const bool target_was_inactive = excess_[arc.to].is_zero();
    arc.residual -= pushed;
    auto& reverse = adjacency_[arc.to][arc.reverse_index];
    reverse.residual = checked_capacity_add(reverse.residual, pushed);
    excess_[from].subtract(amount);
    if (arc.to != source_) {
      excess_[arc.to].add(amount);
      if (target_was_inactive) {
        enqueue_if_active(arc.to);
      }
    }
  }

  void relabel(Vertex vertex) {
    std::size_t minimum = std::numeric_limits<std::size_t>::max();
    for (const auto& arc : adjacency_[vertex]) {
      if (arc.residual > 0) {
        minimum = std::min(minimum, height_[arc.to]);
      }
    }
    if (minimum == std::numeric_limits<std::size_t>::max() ||
        minimum == std::numeric_limits<std::size_t>::max() - 1) {
      throw std::logic_error("push-relabel active vertex has no residual exit");
    }
    height_[vertex] = minimum + 1;
    next_arc_[vertex] = 0;
  }

  void discharge(Vertex vertex) {
    while (!excess_[vertex].is_zero()) {
      if (next_arc_[vertex] == adjacency_[vertex].size()) {
        relabel(vertex);
        continue;
      }
      auto& arc = adjacency_[vertex][next_arc_[vertex]];
      if (arc.residual > 0 && height_[vertex] == height_[arc.to] + 1) {
        push(vertex, arc);
      } else {
        ++next_arc_[vertex];
      }
    }
  }

  std::vector<std::vector<ResidualArc>> adjacency_;
  std::vector<std::size_t> height_;
  std::vector<WideExcess> excess_;
  std::vector<std::size_t> next_arc_;
  std::vector<bool> in_queue_;
  std::queue<Vertex> active_;
  Vertex source_;
  Vertex sink_;
};

}  // namespace push_relabel_detail

// Computes a maximum s-t flow using the FIFO push-relabel preflow algorithm.
//
// This is an independent first-principles baseline beside Dinic. The result
// preserves input edge order and returns both a feasible maximum-flow witness
// and the final residual source-side minimum-cut witness. Self-loops, parallel
// edges, antiparallel edges, and zero-capacity edges are supported.
//
// The direct educational implementation uses current-arc discharge and a FIFO
// active queue without gap/global-relabel heuristics. It conservatively claims
// O(V^2 E) worst-case time and O(V+E) resident algorithm storage.
[[nodiscard]] inline MaxFlowResult push_relabel_max_flow(
    std::size_t vertex_count, std::span<const CapacityEdge> edges,
    Vertex source, Vertex sink) {
  push_relabel_detail::validate_input(vertex_count, edges, source, sink);

  push_relabel_detail::Solver solver(vertex_count, source, sink);
  std::vector<push_relabel_detail::ArcLocation> locations;
  locations.reserve(edges.size());
  for (const auto& edge : edges) {
    locations.push_back(solver.add_edge(edge.from, edge.to, edge.capacity));
  }
  solver.solve();
  const Capacity value = solver.value();
  auto source_side = solver.residual_reachable(source);
  if (source_side[sink]) {
    throw std::logic_error("push-relabel terminated with a residual s-t path");
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
      cut_capacity =
          push_relabel_detail::checked_capacity_add(cut_capacity, edge.capacity);
    }
  }
  if (cut_capacity != value) {
    throw std::logic_error("push-relabel max-flow/min-cut certificate mismatch");
  }
  return MaxFlowResult{value, cut_capacity, std::move(flows),
                       std::move(source_side)};
}

}  // namespace algorithms::graphs
