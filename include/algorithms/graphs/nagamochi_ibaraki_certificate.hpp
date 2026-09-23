#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

// Returns edge ids of a deterministic k-edge-connectivity sparse certificate
// for an undirected multigraph.
//
// Input edges are indexed by their position. Parallel edges are distinct.
// Self-loops are accepted but never selected because they cross no cut.
//
// The construction repeatedly takes a spanning forest of the remaining graph
// and removes those forest edges. The union of the first threshold forests
// satisfies, for every vertex cut S:
//
//   |delta_H(S)| >= min(threshold, |delta_G(S)|).
//
// Since H is a subgraph of G, every cut of size < threshold is preserved
// exactly. The certificate contains at most threshold * (vertex_count - 1)
// edges when that product is representable, and never more than edges.size().
//
// Endpoint validation occurs before any work. threshold == 0 returns an empty
// certificate after validation.
[[nodiscard]] inline std::vector<std::size_t>
nagamochi_ibaraki_sparse_certificate(
    const std::size_t vertex_count,
    const std::span<const std::pair<Vertex, Vertex>> edges,
    const std::size_t threshold) {
  for (const auto& [first, second] : edges) {
    if (first >= vertex_count || second >= vertex_count) {
      throw std::out_of_range(
          "Nagamochi-Ibaraki edge endpoint out of range");
    }
  }

  if (threshold == 0U || vertex_count <= 1U || edges.empty()) {
    return {};
  }

  std::size_t selectable_edges = 0U;
  for (const auto& [first, second] : edges) {
    if (first != second) {
      ++selectable_edges;
    }
  }
  if (selectable_edges == 0U) {
    return {};
  }

  class DisjointSet {
   public:
    explicit DisjointSet(const std::size_t count)
        : parent_(count), size_(count, 1U) {
      std::iota(parent_.begin(), parent_.end(), 0U);
    }

    [[nodiscard]] bool unite(Vertex first, Vertex second) {
      first = find(first);
      second = find(second);
      if (first == second) {
        return false;
      }
      if (size_[first] < size_[second]) {
        std::swap(first, second);
      }
      parent_[second] = first;
      size_[first] += size_[second];
      return true;
    }

   private:
    std::vector<Vertex> parent_;
    std::vector<std::size_t> size_;

    [[nodiscard]] Vertex find(Vertex vertex) {
      Vertex root = vertex;
      while (parent_[root] != root) {
        root = parent_[root];
      }
      while (parent_[vertex] != vertex) {
        const Vertex next = parent_[vertex];
        parent_[vertex] = root;
        vertex = next;
      }
      return root;
    }
  };

  std::vector<unsigned char> remaining(edges.size(), 1U);
  std::vector<std::size_t> certificate;
  certificate.reserve(std::min(edges.size(), selectable_edges));

  const std::size_t rounds = std::min(threshold, selectable_edges);

  for (std::size_t round = 0U; round < rounds; ++round) {
    DisjointSet dsu(vertex_count);
    bool selected_any = false;

    for (std::size_t edge_id = 0U; edge_id < edges.size(); ++edge_id) {
      if (remaining[edge_id] == 0U) {
        continue;
      }

      const auto [first, second] = edges[edge_id];
      if (first == second || !dsu.unite(first, second)) {
        continue;
      }

      remaining[edge_id] = 0U;
      certificate.push_back(edge_id);
      selected_any = true;
    }

    if (!selected_any) {
      break;
    }
  }

  return certificate;
}

}  // namespace algorithms::graphs
