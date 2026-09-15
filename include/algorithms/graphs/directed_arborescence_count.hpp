#pragma once

#include "algorithms/graphs/graph.hpp"
#include "algorithms/number_theory/modular.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace directed_arborescence_count_detail {

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

[[nodiscard]] inline std::size_t reduced_index(Vertex vertex, Vertex root) {
  return vertex < root ? vertex : vertex - 1U;
}

}  // namespace directed_arborescence_count_detail

// Counts rooted spanning out-arborescences modulo a prime.
//
// A selected out-arborescence is directed away from `root`: every non-root
// vertex has exactly one selected incoming arc and every vertex is reachable
// from root through the selected arcs.
//
// Contract:
// - graph must be directed;
// - root must be a valid vertex;
// - stored edge weights are ignored;
// - parallel arc copies count with multiplicity;
// - self-loops are ignored;
// - modulus must be prime.
//
// The directed Matrix-Tree theorem says the determinant of the root-deleted
// cofactor of the in-degree Laplacian D_in - A^T equals this count. Dense
// prime-field elimination gives O(E + V^3 log modulus) time under the
// repository's overflow-safe modular multiplication and O(V^2) storage.
[[nodiscard]] inline std::uint64_t rooted_out_arborescence_count_mod_prime(
    const Graph& graph, Vertex root, std::uint64_t modulus) {
  if (!graph.directed()) {
    throw std::invalid_argument(
        "rooted arborescence count requires a directed graph");
  }
  graph.validate_vertex(root);
  if (!number_theory::is_prime(modulus)) {
    throw std::invalid_argument(
        "rooted arborescence count modulus must be prime");
  }

  const std::size_t vertex_count = graph.vertex_count();
  if (vertex_count == 1U) {
    return 1U % modulus;
  }

  const std::size_t dimension = vertex_count - 1U;
  std::vector<std::vector<std::uint64_t>> cofactor(
      dimension, std::vector<std::uint64_t>(dimension, 0U));

  for (Vertex from = 0; from < vertex_count; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      const Vertex to = edge.to;
      if (from == to || to == root) {
        continue;
      }

      const std::size_t row =
          directed_arborescence_count_detail::reduced_index(to, root);
      cofactor[row][row] = directed_arborescence_count_detail::add_mod(
          cofactor[row][row], 1U, modulus);

      if (from != root) {
        const std::size_t column =
            directed_arborescence_count_detail::reduced_index(from, root);
        cofactor[row][column] =
            directed_arborescence_count_detail::subtract_mod(
                cofactor[row][column], 1U, modulus);
      }
    }
  }

  return directed_arborescence_count_detail::determinant_mod_prime(
      std::move(cofactor), modulus);
}

}  // namespace algorithms::graphs
