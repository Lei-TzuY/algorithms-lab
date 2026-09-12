#pragma once

#include "algorithms/graphs/graph.hpp"
#include "algorithms/number_theory/modular.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace spanning_tree_count_detail {

[[nodiscard]] inline std::uint64_t add_mod(std::uint64_t a,
                                           std::uint64_t b,
                                           std::uint64_t modulus) noexcept {
  return a >= modulus - b ? a - (modulus - b) : a + b;
}

[[nodiscard]] inline std::uint64_t subtract_mod(
    std::uint64_t a, std::uint64_t b, std::uint64_t modulus) noexcept {
  return a >= b ? a - b : modulus - (b - a);
}

[[nodiscard]] inline std::uint64_t determinant_mod_prime(
    std::vector<std::vector<std::uint64_t>> matrix,
    std::uint64_t prime) {
  const std::size_t dimension = matrix.size();
  if (dimension == 0U) {
    return 1U % prime;
  }
  for (const auto& row : matrix) {
    if (row.size() != dimension) {
      throw std::invalid_argument("determinant matrix must be square");
    }
  }

  std::uint64_t determinant = 1U;
  for (std::size_t column = 0; column < dimension; ++column) {
    std::size_t pivot_row = column;
    while (pivot_row < dimension && matrix[pivot_row][column] == 0U) {
      ++pivot_row;
    }
    if (pivot_row == dimension) {
      return 0U;
    }
    if (pivot_row != column) {
      std::swap(matrix[pivot_row], matrix[column]);
      if (determinant != 0U) {
        determinant = prime - determinant;
      }
    }

    const std::uint64_t pivot = matrix[column][column];
    determinant = number_theory::multiply_mod(determinant, pivot, prime);
    const std::uint64_t inverse =
        number_theory::power_mod(pivot, prime - 2U, prime);

    for (std::size_t row = column + 1U; row < dimension; ++row) {
      if (matrix[row][column] == 0U) {
        continue;
      }
      const std::uint64_t factor = number_theory::multiply_mod(
          matrix[row][column], inverse, prime);
      matrix[row][column] = 0U;
      for (std::size_t next = column + 1U; next < dimension; ++next) {
        const std::uint64_t product = number_theory::multiply_mod(
            factor, matrix[column][next], prime);
        matrix[row][next] =
            subtract_mod(matrix[row][next], product, prime);
      }
    }
  }
  return determinant;
}

}  // namespace spanning_tree_count_detail

// Returns the number of spanning trees of an undirected multigraph modulo the
// prime `modulus` through Kirchhoff's matrix-tree theorem.
//
// Contract:
// - directed graphs are rejected;
// - edge weights are ignored: this counts unweighted logical edge copies;
// - parallel edges contribute multiplicity;
// - self-loops never belong to a spanning tree and are ignored;
// - the empty graph returns 0, while a singleton graph returns 1 mod modulus;
// - modulus must be prime.
//
// The implementation constructs one Laplacian cofactor and evaluates its
// determinant by first-principles prime-field elimination. With V vertices and
// E logical non-loop edges, time is O(E + V^3 log modulus) under the repository's
// overflow-safe modular multiplication, with O(V^2) auxiliary storage.
[[nodiscard]] inline std::uint64_t spanning_tree_count_mod_prime(
    const Graph& graph, std::uint64_t modulus) {
  if (graph.directed()) {
    throw std::invalid_argument("spanning-tree count requires an undirected graph");
  }
  if (!number_theory::is_prime(modulus)) {
    throw std::invalid_argument("spanning-tree count modulus must be prime");
  }

  const std::size_t vertex_count = graph.vertex_count();
  if (vertex_count == 0U) {
    return 0U;
  }
  if (vertex_count == 1U) {
    return 1U % modulus;
  }

  const std::size_t dimension = vertex_count - 1U;
  std::vector<std::vector<std::uint64_t>> cofactor(
      dimension, std::vector<std::uint64_t>(dimension, 0U));

  for (Vertex from = 0; from < vertex_count; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      const Vertex to = edge.to;
      if (from >= to) {
        // Self-loops are ignored. Every non-loop undirected logical edge is
        // observed exactly once at its smaller endpoint.
        continue;
      }

      if (from < dimension) {
        cofactor[from][from] = spanning_tree_count_detail::add_mod(
            cofactor[from][from], 1U, modulus);
      }
      if (to < dimension) {
        cofactor[to][to] = spanning_tree_count_detail::add_mod(
            cofactor[to][to], 1U, modulus);
      }
      if (from < dimension && to < dimension) {
        cofactor[from][to] = spanning_tree_count_detail::subtract_mod(
            cofactor[from][to], 1U, modulus);
        cofactor[to][from] = spanning_tree_count_detail::subtract_mod(
            cofactor[to][from], 1U, modulus);
      }
    }
  }

  return spanning_tree_count_detail::determinant_mod_prime(
      std::move(cofactor), modulus);
}

}  // namespace algorithms::graphs
