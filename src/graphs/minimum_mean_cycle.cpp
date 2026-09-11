#include "algorithms/graphs/minimum_mean_cycle.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

constexpr const char* kOverflowMessage =
    "minimum-mean-cycle arithmetic overflow";

std::int64_t checked_add(std::int64_t first, std::int64_t second) {
  if ((second > 0 &&
       first > std::numeric_limits<std::int64_t>::max() - second) ||
      (second < 0 &&
       first < std::numeric_limits<std::int64_t>::min() - second)) {
    throw std::overflow_error(kOverflowMessage);
  }
  return first + second;
}

std::int64_t checked_subtract(std::int64_t first, std::int64_t second) {
  if (second == std::numeric_limits<std::int64_t>::min()) {
    if (first >= 0) {
      throw std::overflow_error(kOverflowMessage);
    }
    return first - second;
  }
  return checked_add(first, -second);
}

std::int64_t checked_multiply_by_u64(std::int64_t value,
                                     std::uint64_t factor) {
  if (value == 0 || factor == 0) {
    return 0;
  }
  if (factor >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error(kOverflowMessage);
  }

  const auto signed_factor = static_cast<std::int64_t>(factor);
  if (value > 0) {
    if (value > std::numeric_limits<std::int64_t>::max() / signed_factor) {
      throw std::overflow_error(kOverflowMessage);
    }
  } else if (value <
             std::numeric_limits<std::int64_t>::min() / signed_factor) {
    throw std::overflow_error(kOverflowMessage);
  }
  return value * signed_factor;
}

std::uint64_t magnitude(std::int64_t value) {
  if (value >= 0) {
    return static_cast<std::uint64_t>(value);
  }
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

// Compares two non-negative fractions without cross multiplication. The
// continued-fraction / Euclidean formulation cannot overflow uint64_t.
int compare_positive_fraction(std::uint64_t first_numerator,
                              std::uint64_t first_denominator,
                              std::uint64_t second_numerator,
                              std::uint64_t second_denominator) {
  bool reversed = false;
  for (;;) {
    const auto first_quotient = first_numerator / first_denominator;
    const auto second_quotient = second_numerator / second_denominator;
    if (first_quotient != second_quotient) {
      const int comparison = first_quotient < second_quotient ? -1 : 1;
      return reversed ? -comparison : comparison;
    }

    const auto first_remainder = first_numerator % first_denominator;
    const auto second_remainder = second_numerator % second_denominator;
    if (first_remainder == 0 || second_remainder == 0) {
      int comparison = 0;
      if (first_remainder == 0 && second_remainder != 0) {
        comparison = -1;
      } else if (first_remainder != 0 && second_remainder == 0) {
        comparison = 1;
      }
      return reversed ? -comparison : comparison;
    }

    first_numerator = first_denominator;
    first_denominator = first_remainder;
    second_numerator = second_denominator;
    second_denominator = second_remainder;
    reversed = !reversed;
  }
}

int compare_fraction(std::int64_t first_numerator,
                     std::uint64_t first_denominator,
                     std::int64_t second_numerator,
                     std::uint64_t second_denominator) {
  const bool first_negative = first_numerator < 0;
  const bool second_negative = second_numerator < 0;
  if (first_negative != second_negative) {
    return first_negative ? -1 : 1;
  }

  const int comparison = compare_positive_fraction(
      magnitude(first_numerator), first_denominator,
      magnitude(second_numerator), second_denominator);
  return first_negative ? -comparison : comparison;
}

struct Ratio {
  std::int64_t numerator{};
  std::uint64_t denominator{1};
};

Ratio reduce_ratio(std::int64_t numerator, std::uint64_t denominator) {
  const auto divisor = std::gcd(magnitude(numerator), denominator);
  if (divisor == 1) {
    return Ratio{numerator, denominator};
  }

  const auto reduced_magnitude = magnitude(numerator) / divisor;
  denominator /= divisor;
  if (numerator < 0) {
    if (reduced_magnitude == (std::uint64_t{1} << 63U)) {
      numerator = std::numeric_limits<std::int64_t>::min();
    } else {
      numerator = -static_cast<std::int64_t>(reduced_magnitude);
    }
  } else {
    numerator = static_cast<std::int64_t>(reduced_magnitude);
  }
  return Ratio{numerator, denominator};
}

struct FlatEdge {
  Vertex from{};
  std::size_t adjacency_index{};
  Vertex to{};
  Weight weight{};
};

std::vector<FlatEdge> flatten_edges(const Graph& graph) {
  std::vector<FlatEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& neighbors = graph.neighbors(from);
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
      edges.push_back(
          FlatEdge{from, index, neighbors[index].to, neighbors[index].weight});
    }
  }
  return edges;
}

}  // namespace

std::optional<MinimumMeanCycleResult> minimum_mean_cycle(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument("minimum mean cycle requires a directed graph");
  }

  const std::size_t vertex_count = graph.vertex_count();
  if (vertex_count == 0) {
    return std::nullopt;
  }
  if (vertex_count >
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
    throw std::length_error("graph too large for exact mean-cycle arithmetic");
  }

  const auto edges = flatten_edges(graph);

  // d[k][v] is the minimum exact weight of any k-edge walk ending at v. All
  // vertices are valid zero-edge starts, equivalent to a conceptual super-source
  // with zero-cost edges whose edges are not counted in k.
  std::vector<std::vector<std::optional<std::int64_t>>> distance(
      vertex_count + 1,
      std::vector<std::optional<std::int64_t>>(vertex_count));
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    distance[0][vertex] = 0;
  }

  for (std::size_t length = 1; length <= vertex_count; ++length) {
    for (const auto& edge : edges) {
      if (!distance[length - 1][edge.from]) {
        continue;
      }
      const auto candidate =
          checked_add(*distance[length - 1][edge.from], edge.weight);
      if (!distance[length][edge.to] ||
          candidate < *distance[length][edge.to]) {
        distance[length][edge.to] = candidate;
      }
    }
  }

  // Karp's theorem:
  //   min_v max_{0 <= k < n} (d[n][v] - d[k][v]) / (n-k)
  bool have_best = false;
  Ratio best_ratio{};
  Vertex best_vertex = 0;
  std::size_t best_reference_length = 0;

  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (!distance[vertex_count][vertex]) {
      continue;
    }

    bool have_vertex_maximum = false;
    Ratio vertex_maximum{};
    std::size_t vertex_reference_length = 0;
    for (std::size_t length = 0; length < vertex_count; ++length) {
      if (!distance[length][vertex]) {
        continue;
      }
      const Ratio candidate{
          checked_subtract(*distance[vertex_count][vertex],
                           *distance[length][vertex]),
          static_cast<std::uint64_t>(vertex_count - length)};
      if (!have_vertex_maximum ||
          compare_fraction(candidate.numerator, candidate.denominator,
                           vertex_maximum.numerator,
                           vertex_maximum.denominator) > 0) {
        vertex_maximum = candidate;
        vertex_reference_length = length;
        have_vertex_maximum = true;
      }
    }

    if (have_vertex_maximum &&
        (!have_best ||
         compare_fraction(vertex_maximum.numerator,
                          vertex_maximum.denominator, best_ratio.numerator,
                          best_ratio.denominator) < 0)) {
      best_ratio = vertex_maximum;
      best_vertex = vertex;
      best_reference_length = vertex_reference_length;
      have_best = true;
    }
  }

  if (!have_best) {
    return std::nullopt;
  }
  best_ratio = reduce_ratio(best_ratio.numerator, best_ratio.denominator);

  // Shift each edge by q*w-p, where p/q is the minimum cycle mean. Every
  // directed cycle now has non-negative shifted total and at least one cycle has
  // shifted total exactly zero.
  std::vector<std::int64_t> shifted_cost(edges.size());
  for (std::size_t index = 0; index < edges.size(); ++index) {
    shifted_cost[index] = checked_subtract(
        checked_multiply_by_u64(edges[index].weight, best_ratio.denominator),
        best_ratio.numerator);
  }

  // Conceptual super-source Bellman-Ford: initialize every potential to zero.
  // At convergence, h[v] <= h[u] + shifted(u,v), so every reduced edge cost is
  // non-negative. A zero-total shifted cycle therefore consists entirely of
  // zero reduced-cost edges.
  std::vector<std::int64_t> potential(vertex_count, 0);
  for (std::size_t pass = 0; pass + 1 < vertex_count; ++pass) {
    bool changed = false;
    for (std::size_t index = 0; index < edges.size(); ++index) {
      const auto& edge = edges[index];
      const auto candidate =
          checked_add(potential[edge.from], shifted_cost[index]);
      if (candidate < potential[edge.to]) {
        potential[edge.to] = candidate;
        changed = true;
      }
    }
    if (!changed) {
      break;
    }
  }

  for (std::size_t index = 0; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const auto candidate =
        checked_add(potential[edge.from], shifted_cost[index]);
    if (candidate < potential[edge.to]) {
      throw std::logic_error("Karp mean permits a negative shifted cycle");
    }
  }

  std::vector<std::vector<std::size_t>> zero_reduced(vertex_count);
  for (std::size_t index = 0; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const auto reduced = checked_subtract(
        checked_add(shifted_cost[index], potential[edge.from]),
        potential[edge.to]);
    if (reduced < 0) {
      throw std::logic_error("negative reduced cost after Bellman-Ford");
    }
    if (reduced == 0) {
      zero_reduced[edge.from].push_back(index);
    }
  }

  // Find one concrete directed cycle in the zero-reduced-cost subgraph. Stable
  // vertex and adjacency iteration makes the witness deterministic.
  std::vector<unsigned char> color(vertex_count, 0);
  std::vector<Vertex> parent(vertex_count, vertex_count);
  std::vector<std::size_t> parent_edge(vertex_count, edges.size());
  struct Frame {
    Vertex vertex{};
    std::size_t next_edge{};
  };
  std::vector<Frame> stack;
  std::vector<std::size_t> cycle_edge_ids;

  for (Vertex start = 0;
       start < vertex_count && cycle_edge_ids.empty(); ++start) {
    if (color[start] != 0) {
      continue;
    }
    color[start] = 1;
    stack.push_back(Frame{start, 0});

    while (!stack.empty() && cycle_edge_ids.empty()) {
      auto& frame = stack.back();
      if (frame.next_edge >= zero_reduced[frame.vertex].size()) {
        color[frame.vertex] = 2;
        stack.pop_back();
        continue;
      }

      const auto edge_id = zero_reduced[frame.vertex][frame.next_edge++];
      const auto to = edges[edge_id].to;
      if (color[to] == 0) {
        parent[to] = frame.vertex;
        parent_edge[to] = edge_id;
        color[to] = 1;
        stack.push_back(Frame{to, 0});
      } else if (color[to] == 1) {
        std::vector<std::size_t> reversed_path;
        Vertex current = frame.vertex;
        while (current != to) {
          if (current >= vertex_count || parent_edge[current] >= edges.size()) {
            throw std::logic_error(
                "failed to reconstruct zero-reduced-cost cycle");
          }
          reversed_path.push_back(parent_edge[current]);
          current = parent[current];
        }
        std::reverse(reversed_path.begin(), reversed_path.end());
        cycle_edge_ids = std::move(reversed_path);
        cycle_edge_ids.push_back(edge_id);
      }
    }
    stack.clear();
  }

  if (cycle_edge_ids.empty()) {
    throw std::logic_error("minimum mean cycle witness was not found");
  }

  std::int64_t cycle_weight = 0;
  std::int64_t shifted_sum = 0;
  std::vector<MeanCycleEdge> witness;
  witness.reserve(cycle_edge_ids.size());
  for (const auto edge_id : cycle_edge_ids) {
    const auto& edge = edges[edge_id];
    cycle_weight = checked_add(cycle_weight, edge.weight);
    shifted_sum = checked_add(shifted_sum, shifted_cost[edge_id]);
    witness.push_back(MeanCycleEdge{edge.from, edge.adjacency_index, edge.to,
                                    edge.weight});
  }
  if (shifted_sum != 0) {
    throw std::logic_error("reconstructed cycle does not attain Karp mean");
  }

  return MinimumMeanCycleResult{
      best_ratio.numerator, best_ratio.denominator, cycle_weight,
      std::move(witness), best_vertex, best_reference_length};
}

}  // namespace algorithms::graphs
