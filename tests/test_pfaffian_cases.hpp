#pragma once

#include "algorithms/linear_algebra/pfaffian.hpp"
#include "algorithms/number_theory/modular.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pfaffian_tests {
using Matrix = std::vector<std::vector<std::uint64_t>>;

[[nodiscard]] inline std::uint64_t add_mod(std::uint64_t first,
                                           std::uint64_t second,
                                           std::uint64_t modulus) {
  return first >= modulus - second ? first - (modulus - second)
                                   : first + second;
}

[[nodiscard]] inline std::uint64_t subtract_mod(std::uint64_t first,
                                                std::uint64_t second,
                                                std::uint64_t modulus) {
  return first >= second ? first - second : modulus - (second - first);
}

[[nodiscard]] inline std::uint64_t recursive_pairing_pfaffian(
    const Matrix& matrix, std::uint64_t modulus) {
  const std::size_t dimension = matrix.size();
  if (dimension == 0U) {
    return 1U % modulus;
  }
  if ((dimension & 1U) != 0U) {
    return 0U;
  }

  std::uint64_t result = 0U;
  for (std::size_t partner = 1U; partner < dimension; ++partner) {
    Matrix remainder;
    remainder.reserve(dimension - 2U);
    for (std::size_t row = 1U; row < dimension; ++row) {
      if (row == partner) {
        continue;
      }
      std::vector<std::uint64_t> reduced_row;
      reduced_row.reserve(dimension - 2U);
      for (std::size_t column = 1U; column < dimension; ++column) {
        if (column != partner) {
          reduced_row.push_back(matrix[row][column] % modulus);
        }
      }
      remainder.push_back(std::move(reduced_row));
    }
    const std::uint64_t term = algorithms::number_theory::multiply_mod(
        matrix[0U][partner] % modulus,
        recursive_pairing_pfaffian(remainder, modulus), modulus);
    result = (partner & 1U) != 0U ? add_mod(result, term, modulus)
                                  : subtract_mod(result, term, modulus);
  }
  return result;
}

[[nodiscard]] inline Matrix random_skew_matrix(std::size_t dimension,
                                               std::uint64_t modulus,
                                               std::mt19937_64& generator) {
  Matrix matrix(dimension, std::vector<std::uint64_t>(dimension, 0U));
  for (std::size_t row = 0U; row < dimension; ++row) {
    for (std::size_t column = row + 1U; column < dimension; ++column) {
      const std::uint64_t value = generator() % modulus;
      matrix[row][column] = value;
      matrix[column][row] = value == 0U ? 0U : modulus - value;
    }
  }
  return matrix;
}

[[nodiscard]] inline unsigned int permutation_inversions(
    const std::vector<std::size_t>& permutation) {
  unsigned int inversions = 0U;
  for (std::size_t left = 0U; left < permutation.size(); ++left) {
    for (std::size_t right = left + 1U; right < permutation.size(); ++right) {
      if (permutation[left] > permutation[right]) {
        ++inversions;
      }
    }
  }
  return inversions;
}

[[nodiscard]] inline std::uint64_t permutation_determinant(
    const Matrix& matrix, std::uint64_t modulus) {
  const std::size_t dimension = matrix.size();
  std::vector<std::size_t> permutation(dimension);
  std::iota(permutation.begin(), permutation.end(), 0U);
  std::uint64_t determinant = 0U;
  do {
    std::uint64_t term = 1U;
    for (std::size_t row = 0U; row < dimension; ++row) {
      term = algorithms::number_theory::multiply_mod(
          term, matrix[row][permutation[row]] % modulus, modulus);
    }
    if ((permutation_inversions(permutation) & 1U) == 0U) {
      determinant = add_mod(determinant, term, modulus);
    } else {
      determinant = subtract_mod(determinant, term, modulus);
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return determinant;
}
}  // namespace pfaffian_tests

TEST_CASE(pfaffian_validates_prime_field_and_skew_shape) {
  using algorithms::linear_algebra::pfaffian_mod_prime;
  using pfaffian_tests::Matrix;
  REQUIRE_EQ(pfaffian_mod_prime({}, 17U), 1U);
  REQUIRE_EQ(pfaffian_mod_prime({}, 2U), 1U);
  REQUIRE_THROWS_AS(pfaffian_mod_prime({}, 15U), std::invalid_argument);
  REQUIRE_THROWS_AS(pfaffian_mod_prime(Matrix{{0U, 1U}, {16U}}, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(pfaffian_mod_prime(Matrix{{1U}}, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(pfaffian_mod_prime(Matrix{{0U, 1U}, {1U, 0U}}, 17U),
                    std::invalid_argument);
}

TEST_CASE(pfaffian_handles_known_values_odd_dimension_and_pivot_swap) {
  using algorithms::linear_algebra::pfaffian_mod_prime;
  using pfaffian_tests::Matrix;
  const Matrix two{{0U, 5U}, {12U, 0U}};
  REQUIRE_EQ(pfaffian_mod_prime(two, 17U), 5U);

  const Matrix four{{0U, 2U, 3U, 4U},
                    {15U, 0U, 5U, 6U},
                    {14U, 12U, 0U, 7U},
                    {13U, 11U, 10U, 0U}};
  REQUIRE_EQ(pfaffian_mod_prime(four, 17U), 16U);

  // a01 = 0 forces a simultaneous row/column pivot swap. Expansion gives
  // -a02*a13 + a03*a12 = -3*5 + 4*6 = 9 (mod 17).
  const Matrix swap_case{{0U, 0U, 3U, 4U},
                         {0U, 0U, 6U, 5U},
                         {14U, 11U, 0U, 7U},
                         {13U, 12U, 10U, 0U}};
  REQUIRE_EQ(pfaffian_mod_prime(swap_case, 17U), 9U);

  const Matrix odd{{0U, 1U, 2U}, {16U, 0U, 3U}, {15U, 14U, 0U}};
  REQUIRE_EQ(pfaffian_mod_prime(odd, 17U), 0U);
}

TEST_CASE(pfaffian_normalizes_entries_and_supports_full_width_prime) {
  using algorithms::linear_algebra::pfaffian_mod_prime;
  using pfaffian_tests::Matrix;
  const Matrix normalized{{0U, 22U}, {29U, 0U}};  // 22=5, 29=12 mod 17.
  REQUIRE_EQ(pfaffian_mod_prime(normalized, 17U), 5U);

  constexpr std::uint64_t prime = 18446744073709551557ULL;
  const Matrix matrix{{0U, prime - 2U}, {2U, 0U}};
  REQUIRE_EQ(pfaffian_mod_prime(matrix, prime), prime - 2U);
}

TEST_CASE(pfaffian_randomized_matches_independent_pairing_expansion) {
  using algorithms::linear_algebra::pfaffian_mod_prime;
  using pfaffian_tests::random_skew_matrix;
  using pfaffian_tests::recursive_pairing_pfaffian;

  constexpr std::array<std::uint64_t, 7> primes{
      2U, 3U, 5U, 17U, 97U, 1000000007ULL, 18446744073709551557ULL};
  std::mt19937_64 generator(0x504641464649414eULL);
  for (std::size_t trial = 0U; trial < 1200U; ++trial) {
    const std::size_t dimension = static_cast<std::size_t>(generator() % 9U);
    const std::uint64_t modulus = primes[generator() % primes.size()];
    const auto matrix = random_skew_matrix(dimension, modulus, generator);
    REQUIRE_EQ(pfaffian_mod_prime(matrix, modulus),
               recursive_pairing_pfaffian(matrix, modulus));
  }
}

TEST_CASE(pfaffian_square_matches_independent_permutation_determinant) {
  using algorithms::linear_algebra::pfaffian_mod_prime;
  using algorithms::number_theory::multiply_mod;
  using pfaffian_tests::permutation_determinant;
  using pfaffian_tests::random_skew_matrix;

  constexpr std::uint64_t modulus = 101U;
  std::mt19937_64 generator(0x4445545046414646ULL);
  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    const std::size_t dimension = 2U * static_cast<std::size_t>(generator() % 4U);
    const auto matrix = random_skew_matrix(dimension, modulus, generator);
    const std::uint64_t pfaffian = pfaffian_mod_prime(matrix, modulus);
    REQUIRE_EQ(multiply_mod(pfaffian, pfaffian, modulus),
               permutation_determinant(matrix, modulus));
  }
}
