#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace algorithms::linear_algebra {

using PrimeFieldMatrix = std::vector<std::vector<std::uint64_t>>;

struct StrassenMatrixProduct {
  PrimeFieldMatrix product;
  std::size_t padded_dimension{0U};
  std::size_t recursive_calls{0U};
  std::size_t strassen_nodes{0U};
  std::size_t scalar_products{0U};

  friend bool operator==(const StrassenMatrixProduct&,
                         const StrassenMatrixProduct&) = default;
};

namespace strassen_detail {

inline void checked_increment(std::size_t& value, const char* what) {
  if (value == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error(std::string(what) + " counter overflow");
  }
  ++value;
}

[[nodiscard]] inline std::uint64_t add_mod(const std::uint64_t left,
                                           const std::uint64_t right,
                                           const std::uint64_t modulus) noexcept {
  if (left >= modulus - right) {
    return left - (modulus - right);
  }
  return left + right;
}

[[nodiscard]] inline std::uint64_t subtract_mod(
    const std::uint64_t left, const std::uint64_t right,
    const std::uint64_t modulus) noexcept {
  if (left >= right) {
    return left - right;
  }
  return modulus - (right - left);
}

inline void validate_rectangular_nonempty(const PrimeFieldMatrix& matrix,
                                          const char* name) {
  if (matrix.empty() || matrix.front().empty()) {
    throw std::invalid_argument(std::string(name) + " must be non-empty");
  }
  const std::size_t columns = matrix.front().size();
  for (const auto& row : matrix) {
    if (row.size() != columns) {
      throw std::invalid_argument(std::string(name) + " must be rectangular");
    }
  }
}

inline void validate_residues(const PrimeFieldMatrix& matrix,
                              const std::uint64_t modulus,
                              const char* name) {
  for (const auto& row : matrix) {
    for (const std::uint64_t value : row) {
      if (value >= modulus) {
        throw std::invalid_argument(std::string(name) +
                                    " contains a non-canonical residue");
      }
    }
  }
}

[[nodiscard]] inline std::size_t next_power_of_two(std::size_t required) {
  std::size_t result = 1U;
  while (result < required) {
    if (result > std::numeric_limits<std::size_t>::max() / 2U) {
      throw std::length_error("Strassen padded dimension is not representable");
    }
    result *= 2U;
  }
  if (result > std::numeric_limits<std::size_t>::max() / result) {
    throw std::length_error("Strassen padded matrix area is not representable");
  }
  return result;
}

[[nodiscard]] inline PrimeFieldMatrix make_square(const std::size_t size) {
  return PrimeFieldMatrix(size, std::vector<std::uint64_t>(size, 0U));
}

[[nodiscard]] inline PrimeFieldMatrix block(const PrimeFieldMatrix& matrix,
                                            const std::size_t row_begin,
                                            const std::size_t column_begin,
                                            const std::size_t size) {
  PrimeFieldMatrix result = make_square(size);
  for (std::size_t row = 0U; row < size; ++row) {
    for (std::size_t column = 0U; column < size; ++column) {
      result[row][column] = matrix[row_begin + row][column_begin + column];
    }
  }
  return result;
}

[[nodiscard]] inline PrimeFieldMatrix add(const PrimeFieldMatrix& left,
                                          const PrimeFieldMatrix& right,
                                          const std::uint64_t modulus) {
  const std::size_t size = left.size();
  PrimeFieldMatrix result = make_square(size);
  for (std::size_t row = 0U; row < size; ++row) {
    for (std::size_t column = 0U; column < size; ++column) {
      result[row][column] = add_mod(left[row][column], right[row][column], modulus);
    }
  }
  return result;
}

[[nodiscard]] inline PrimeFieldMatrix subtract(const PrimeFieldMatrix& left,
                                               const PrimeFieldMatrix& right,
                                               const std::uint64_t modulus) {
  const std::size_t size = left.size();
  PrimeFieldMatrix result = make_square(size);
  for (std::size_t row = 0U; row < size; ++row) {
    for (std::size_t column = 0U; column < size; ++column) {
      result[row][column] =
          subtract_mod(left[row][column], right[row][column], modulus);
    }
  }
  return result;
}

struct Diagnostics {
  std::size_t recursive_calls{0U};
  std::size_t strassen_nodes{0U};
  std::size_t scalar_products{0U};
};

[[nodiscard]] inline PrimeFieldMatrix classical_square_product(
    const PrimeFieldMatrix& left, const PrimeFieldMatrix& right,
    const std::uint64_t modulus, Diagnostics& diagnostics) {
  const std::size_t size = left.size();
  PrimeFieldMatrix result = make_square(size);
  for (std::size_t row = 0U; row < size; ++row) {
    for (std::size_t shared = 0U; shared < size; ++shared) {
      for (std::size_t column = 0U; column < size; ++column) {
        checked_increment(diagnostics.scalar_products, "Strassen scalar product");
        const std::uint64_t term = algorithms::number_theory::multiply_mod(
            left[row][shared], right[shared][column], modulus);
        result[row][column] = add_mod(result[row][column], term, modulus);
      }
    }
  }
  return result;
}

[[nodiscard]] inline PrimeFieldMatrix recursive_product(
    const PrimeFieldMatrix& left, const PrimeFieldMatrix& right,
    const std::uint64_t modulus, const std::size_t leaf_size,
    Diagnostics& diagnostics) {
  checked_increment(diagnostics.recursive_calls, "Strassen recursive call");
  const std::size_t size = left.size();
  if (size <= leaf_size) {
    return classical_square_product(left, right, modulus, diagnostics);
  }

  checked_increment(diagnostics.strassen_nodes, "Strassen node");
  const std::size_t half = size / 2U;

  const PrimeFieldMatrix a11 = block(left, 0U, 0U, half);
  const PrimeFieldMatrix a12 = block(left, 0U, half, half);
  const PrimeFieldMatrix a21 = block(left, half, 0U, half);
  const PrimeFieldMatrix a22 = block(left, half, half, half);
  const PrimeFieldMatrix b11 = block(right, 0U, 0U, half);
  const PrimeFieldMatrix b12 = block(right, 0U, half, half);
  const PrimeFieldMatrix b21 = block(right, half, 0U, half);
  const PrimeFieldMatrix b22 = block(right, half, half, half);

  const PrimeFieldMatrix m1 = recursive_product(
      add(a11, a22, modulus), add(b11, b22, modulus), modulus, leaf_size,
      diagnostics);
  const PrimeFieldMatrix m2 = recursive_product(
      add(a21, a22, modulus), b11, modulus, leaf_size, diagnostics);
  const PrimeFieldMatrix m3 = recursive_product(
      a11, subtract(b12, b22, modulus), modulus, leaf_size, diagnostics);
  const PrimeFieldMatrix m4 = recursive_product(
      a22, subtract(b21, b11, modulus), modulus, leaf_size, diagnostics);
  const PrimeFieldMatrix m5 = recursive_product(
      add(a11, a12, modulus), b22, modulus, leaf_size, diagnostics);
  const PrimeFieldMatrix m6 = recursive_product(
      subtract(a21, a11, modulus), add(b11, b12, modulus), modulus, leaf_size,
      diagnostics);
  const PrimeFieldMatrix m7 = recursive_product(
      subtract(a12, a22, modulus), add(b21, b22, modulus), modulus, leaf_size,
      diagnostics);

  PrimeFieldMatrix c11 = add(m1, m4, modulus);
  c11 = subtract(c11, m5, modulus);
  c11 = add(c11, m7, modulus);
  const PrimeFieldMatrix c12 = add(m3, m5, modulus);
  const PrimeFieldMatrix c21 = add(m2, m4, modulus);
  PrimeFieldMatrix c22 = subtract(m1, m2, modulus);
  c22 = add(c22, m3, modulus);
  c22 = add(c22, m6, modulus);

  PrimeFieldMatrix result = make_square(size);
  for (std::size_t row = 0U; row < half; ++row) {
    for (std::size_t column = 0U; column < half; ++column) {
      result[row][column] = c11[row][column];
      result[row][column + half] = c12[row][column];
      result[row + half][column] = c21[row][column];
      result[row + half][column + half] = c22[row][column];
    }
  }
  return result;
}

}  // namespace strassen_detail

// Exact matrix multiplication over the prime field F_modulus using the
// seven-product Strassen recurrence on a zero-padded power-of-two square.
// `leaf_size` selects when recursion switches to the ordinary cubic kernel.
[[nodiscard]] inline StrassenMatrixProduct strassen_matrix_multiply_mod(
    const PrimeFieldMatrix& first, const PrimeFieldMatrix& second,
    const std::uint64_t modulus, const std::size_t leaf_size = 32U) {
  if (!algorithms::number_theory::is_prime(modulus)) {
    throw std::invalid_argument("Strassen modulus must be prime");
  }
  if (leaf_size == 0U) {
    throw std::invalid_argument("Strassen leaf size must be positive");
  }
  strassen_detail::validate_rectangular_nonempty(first, "first matrix");
  strassen_detail::validate_rectangular_nonempty(second, "second matrix");

  const std::size_t rows = first.size();
  const std::size_t shared = first.front().size();
  const std::size_t columns = second.front().size();
  if (second.size() != shared) {
    throw std::invalid_argument("Strassen matrix shapes are incompatible");
  }
  strassen_detail::validate_residues(first, modulus, "first matrix");
  strassen_detail::validate_residues(second, modulus, "second matrix");

  const std::size_t padded_dimension =
      strassen_detail::next_power_of_two(std::max({rows, shared, columns}));
  PrimeFieldMatrix padded_first =
      strassen_detail::make_square(padded_dimension);
  PrimeFieldMatrix padded_second =
      strassen_detail::make_square(padded_dimension);
  for (std::size_t row = 0U; row < rows; ++row) {
    for (std::size_t column = 0U; column < shared; ++column) {
      padded_first[row][column] = first[row][column];
    }
  }
  for (std::size_t row = 0U; row < shared; ++row) {
    for (std::size_t column = 0U; column < columns; ++column) {
      padded_second[row][column] = second[row][column];
    }
  }

  strassen_detail::Diagnostics diagnostics;
  const PrimeFieldMatrix padded_product = strassen_detail::recursive_product(
      padded_first, padded_second, modulus, leaf_size, diagnostics);

  PrimeFieldMatrix product(rows, std::vector<std::uint64_t>(columns, 0U));
  for (std::size_t row = 0U; row < rows; ++row) {
    for (std::size_t column = 0U; column < columns; ++column) {
      product[row][column] = padded_product[row][column];
    }
  }

  return StrassenMatrixProduct{std::move(product),
                               padded_dimension,
                               diagnostics.recursive_calls,
                               diagnostics.strassen_nodes,
                               diagnostics.scalar_products};
}

}  // namespace algorithms::linear_algebra
