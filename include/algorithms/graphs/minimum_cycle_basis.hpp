#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct CycleBasisEdgeRef {
  Vertex from;
  std::size_t adjacency_index;
  Vertex to;
  Weight weight;

  friend bool operator==(const CycleBasisEdgeRef&, const CycleBasisEdgeRef&) = default;
};

struct CycleBasisCycle {
  std::vector<std::size_t> logical_edge_ids;
  std::uint64_t weight;

  friend bool operator==(const CycleBasisCycle&, const CycleBasisCycle&) = default;
};

struct MinimumCycleBasisResult {
  std::vector<CycleBasisEdgeRef> logical_edges;
  std::vector<CycleBasisCycle> cycles;
  std::size_t cycle_space_dimension{};
  std::uint64_t total_weight{};

  friend bool operator==(const MinimumCycleBasisResult&, const MinimumCycleBasisResult&) = default;
};

namespace minimum_cycle_basis_detail {

using Bits = std::vector<std::uint64_t>;

inline bool bit_test(const Bits& bits, std::size_t index) {
  return ((bits[index / 64U] >> (index % 64U)) & 1U) != 0U;
}
inline void bit_flip(Bits& bits, std::size_t index) {
  bits[index / 64U] ^= std::uint64_t{1} << (index % 64U);
}
inline void bits_xor(Bits& into, const Bits& other) {
  for (std::size_t i = 0; i < into.size(); ++i) into[i] ^= other[i];
}
inline bool bits_zero(const Bits& bits) {
  for (const auto word : bits) if (word != 0U) return false;
  return true;
}
inline std::vector<std::size_t> ids_from_bits(const Bits& bits, std::size_t count) {
  std::vector<std::size_t> ids;
  for (std::size_t i = 0; i < count; ++i) if (bit_test(bits, i)) ids.push_back(i);
  return ids;
}
inline std::optional<std::uint64_t> add_u64(std::uint64_t first, std::uint64_t second) {
  if (first > std::numeric_limits<std::uint64_t>::max() - second) return std::nullopt;
  return first + second;
}
inline std::vector<CycleBasisEdgeRef> logical_edges(const Graph& graph) {
  if (graph.directed()) throw std::invalid_argument("minimum cycle basis requires an undirected graph");
  std::vector<CycleBasisEdgeRef> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& neighbors = graph.neighbors(from);
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
      const auto& edge = neighbors[index];
      if (edge.weight <= 0) throw std::invalid_argument("minimum cycle basis requires positive weights");
      if (from <= edge.to) edges.push_back(CycleBasisEdgeRef{from, index, edge.to, edge.weight});
    }
  }
  return edges;
}
inline std::size_t component_count(std::size_t vertex_count,
                                   const std::vector<CycleBasisEdgeRef>& edges) {
  std::vector<std::size_t> parent(vertex_count);
  std::vector<std::size_t> size(vertex_count, 1U);
  for (std::size_t i = 0; i < vertex_count; ++i) parent[i] = i;
  auto find = [&](std::size_t vertex) {
    std::size_t root = vertex;
    while (parent[root] != root) root = parent[root];
    while (parent[vertex] != vertex) {
      const auto next = parent[vertex];
      parent[vertex] = root;
      vertex = next;
    }
    return root;
  };
  for (const auto& edge : edges) {
    if (edge.from == edge.to) continue;
    auto first = find(edge.from);
    auto second = find(edge.to);
    if (first == second) continue;
    if (size[first] < size[second]) std::swap(first, second);
    parent[second] = first;
    size[first] += size[second];
  }
  std::size_t count = 0;
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (find(vertex) == vertex) ++count;
  }
  return count;
}
inline bool add_if_independent(const Bits& input, std::vector<std::optional<Bits>>& basis) {
  Bits reduced = input;
  for (std::size_t pivot = basis.size(); pivot-- > 0;) {
    if (!bit_test(reduced, pivot)) continue;
    if (basis[pivot].has_value()) bits_xor(reduced, *basis[pivot]);
    else { basis[pivot] = std::move(reduced); return true; }
  }
  return false;
}
struct Candidate { Bits bits; std::uint64_t weight; std::vector<std::size_t> ids; };

}  // namespace minimum_cycle_basis_detail

// Exact minimum-weight cycle basis for an undirected positive-weight multigraph.
// Parallel copies and self-loops remain distinct logical edges. Returned cycles
// are GF(2) edge vectors represented by sorted logical-edge ids.
[[nodiscard]] inline MinimumCycleBasisResult minimum_weight_cycle_basis(const Graph& graph) {
  using namespace minimum_cycle_basis_detail;
  MinimumCycleBasisResult result;
  result.logical_edges = logical_edges(graph);
  const std::size_t n = graph.vertex_count();
  const std::size_t m = result.logical_edges.size();
  const std::size_t components = component_count(n, result.logical_edges);
  result.cycle_space_dimension = m - (n - components);
  if (result.cycle_space_dimension == 0U) return result;

  const std::size_t words = (m + 63U) / 64U;
  struct Incident { Vertex to; std::uint64_t weight; std::size_t edge_id; };
  std::vector<std::vector<Incident>> adjacency(n);
  for (std::size_t id = 0; id < m; ++id) {
    const auto& edge = result.logical_edges[id];
    const auto weight = static_cast<std::uint64_t>(edge.weight);
    adjacency[edge.from].push_back(Incident{edge.to, weight, id});
    if (edge.from != edge.to) adjacency[edge.to].push_back(Incident{edge.from, weight, id});
  }

  std::vector<Candidate> candidates;
  for (Vertex root = 0; root < n; ++root) {
    std::vector<std::optional<std::uint64_t>> distance(n);
    std::vector<std::optional<Vertex>> parent(n);
    std::vector<std::optional<std::size_t>> parent_edge(n);
    std::vector<bool> settled(n, false);
    std::vector<Vertex> settle_order;
    distance[root] = 0U;
    for (std::size_t step = 0; step < n; ++step) {
      std::optional<Vertex> best;
      for (Vertex vertex = 0; vertex < n; ++vertex) {
        if (settled[vertex] || !distance[vertex].has_value()) continue;
        if (!best.has_value() || *distance[vertex] < *distance[*best] ||
            (*distance[vertex] == *distance[*best] && vertex < *best)) best = vertex;
      }
      if (!best.has_value()) break;
      const Vertex from = *best;
      settled[from] = true;
      settle_order.push_back(from);
      for (const auto& incident : adjacency[from]) {
        if (settled[incident.to]) continue;
        const auto proposed = add_u64(*distance[from], incident.weight);
        if (!proposed.has_value()) continue;
        if (!distance[incident.to].has_value() || *proposed < *distance[incident.to]) {
          distance[incident.to] = *proposed;
          parent[incident.to] = from;
          parent_edge[incident.to] = incident.edge_id;
        }
      }
    }

    std::vector<Bits> path_bits(n, Bits(words, 0U));
    for (const Vertex vertex : settle_order) {
      if (vertex == root || !parent[vertex].has_value()) continue;
      path_bits[vertex] = path_bits[*parent[vertex]];
      bit_flip(path_bits[vertex], *parent_edge[vertex]);
    }
    for (std::size_t edge_id = 0; edge_id < m; ++edge_id) {
      const auto& edge = result.logical_edges[edge_id];
      if (!distance[edge.from].has_value() || !distance[edge.to].has_value()) continue;
      Bits bits = path_bits[edge.from];
      bits_xor(bits, path_bits[edge.to]);
      bit_flip(bits, edge_id);
      if (bits_zero(bits)) continue;
      std::uint64_t weight = 0U;
      bool representable = true;
      for (std::size_t id = 0; id < m; ++id) {
        if (!bit_test(bits, id)) continue;
        const auto sum = add_u64(weight, static_cast<std::uint64_t>(result.logical_edges[id].weight));
        if (!sum.has_value()) { representable = false; break; }
        weight = *sum;
      }
      if (!representable) continue;
      candidates.push_back(Candidate{bits, weight, ids_from_bits(bits, m)});
    }
  }

  std::sort(candidates.begin(), candidates.end(), [](const Candidate& first, const Candidate& second) {
    if (first.weight != second.weight) return first.weight < second.weight;
    return first.ids < second.ids;
  });
  candidates.erase(std::unique(candidates.begin(), candidates.end(),
                               [](const Candidate& first, const Candidate& second) { return first.ids == second.ids; }),
                   candidates.end());

  std::vector<std::optional<Bits>> basis(m);
  for (const auto& candidate : candidates) {
    if (!add_if_independent(candidate.bits, basis)) continue;
    const auto total = add_u64(result.total_weight, candidate.weight);
    if (!total.has_value()) throw std::overflow_error("minimum cycle basis total weight exceeds uint64_t");
    result.total_weight = *total;
    result.cycles.push_back(CycleBasisCycle{candidate.ids, candidate.weight});
    if (result.cycles.size() == result.cycle_space_dimension) break;
  }
  if (result.cycles.size() != result.cycle_space_dimension) {
    throw std::overflow_error("no representable minimum cycle basis exists in uint64_t");
  }
  return result;
}

}  // namespace algorithms::graphs
