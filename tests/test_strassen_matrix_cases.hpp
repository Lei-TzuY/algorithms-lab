#pragma once

#include "algorithms/linear_algebra/strassen_matrix.hpp"
#include "algorithms/randomized/freivalds.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace strassen_test_detail {
using algorithms::linear_algebra::PrimeFieldMatrix;

[[nodiscard]] PrimeFieldMatrix cubic_oracle(const PrimeFieldMatrix& first,
                                            const PrimeFieldMatrix& second,
                                            const std::uint64_t modulus) {
  const std::size_t rows = first.size();
  const std::size_t shared = first.front().size();
  const std::size_t columns = second.front().size();
  PrimeFieldMatrix result(rows, std::vector<std::uint64_t>(columns, 0U));
  for (std::size_t row = 0U; row < rows; ++row) {
    for (std::size_t shared_index = 0U; shared_index < shared; ++shared_index) {
      for (std::size_t column = 0U; column < columns; ++column) {
        // Randomized oracle calls use modulus 101 and dimensions <= 8, so the
        // ordinary product/add below are proven far below uint64_t overflow.
        const std::uint64_t term =
            (first[row][shared_index] * second[shared_index][column]) % modulus;
        result[row][column] = (result[row][column] + term) % modulus;
      }
    }
  }
  return result;
}

[[nodiscard]] PrimeFieldMatrix random_matrix(std::mt19937_64& random,
                                             const std::size_t rows,
                                             const std::size_t columns,
                                             const std::uint64_t modulus) {
  PrimeFieldMatrix result(rows, std::vector<std::uint64_t>(columns, 0U));
  for (auto& row : result) {
    for (std::uint64_t& value : row) {
      value = random() % modulus;
    }
  }
  return result;
}
}  // namespace strassen_test_detail

TEST_CASE(strassen_validation_and_rectangular_padding) {
  using algorithms::linear_algebra::PrimeFieldMatrix;
  using algorithms::linear_algebra::strassen_matrix_multiply_mod;

  const PrimeFieldMatrix first{{1U, 2U, 3U, 4U, 5U},
                               {6U, 7U, 8U, 9U, 10U},
                               {11U, 12U, 13U, 14U, 15U}};
  const PrimeFieldMatrix second{{2U, 3U}, {4U, 5U}, {6U, 7U},
                                {8U, 9U}, {10U, 11U}};
  const auto result = strassen_matrix_multiply_mod(first, second, 101U, 1U);
  REQUIRE_EQ(result.product,
             strassen_test_detail::cubic_oracle(first, second, 101U));
  REQUIRE_EQ(result.padded_dimension, 8U);
  REQUIRE(result.strassen_nodes > 0U);

  REQUIRE_THROWS_AS(strassen_matrix_multiply_mod({}, second, 101U, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      strassen_matrix_multiply_mod(PrimeFieldMatrix{{1U}, {2U, 3U}},
                                   PrimeFieldMatrix{{1U}}, 101U, 1U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      strassen_matrix_multiply_mod(PrimeFieldMatrix{{1U, 2U}},
                                   PrimeFieldMatrix{{1U, 2U}}, 101U, 1U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      strassen_matrix_multiply_mod(PrimeFieldMatrix{{101U}},
                                   PrimeFieldMatrix{{1U}}, 101U, 1U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      strassen_matrix_multiply_mod(PrimeFieldMatrix{{1U}},
                                   PrimeFieldMatrix{{1U}}, 100U, 1U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      strassen_matrix_multiply_mod(PrimeFieldMatrix{{1U}},
                                   PrimeFieldMatrix{{1U}}, 101U, 0U),
      std::invalid_argument);
}

TEST_CASE(strassen_forced_seven_product_diagnostics) {
  using algorithms::linear_algebra::PrimeFieldMatrix;
  using algorithms::linear_algebra::strassen_matrix_multiply_mod;

  PrimeFieldMatrix first(8U, std::vector<std::uint64_t>(8U, 0U));
  PrimeFieldMatrix second(8U, std::vector<std::uint64_t>(8U, 0U));
  for (std::size_t row = 0U; row < 8U; ++row) {
    for (std::size_t column = 0U; column < 8U; ++column) {
      first[row][column] = static_cast<std::uint64_t>((row * 11U + column * 7U) % 101U);
      second[row][column] = static_cast<std::uint64_t>((row * 13U + column * 5U + 3U) % 101U);
    }
  }

  const auto recursive = strassen_matrix_multiply_mod(first, second, 101U, 1U);
  const auto leaf = strassen_matrix_multiply_mod(first, second, 101U, 8U);
  REQUIRE_EQ(recursive.product, leaf.product);
  REQUIRE_EQ(recursive.product,
             strassen_test_detail::cubic_oracle(first, second, 101U));
  REQUIRE_EQ(recursive.padded_dimension, 8U);
  REQUIRE_EQ(recursive.strassen_nodes, 57U);
  REQUIRE_EQ(recursive.recursive_calls, 400U);
  REQUIRE_EQ(recursive.scalar_products, 343U);
  REQUIRE_EQ(leaf.strassen_nodes, 0U);
  REQUIRE_EQ(leaf.recursive_calls, 1U);
  REQUIRE_EQ(leaf.scalar_products, 512U);
}

TEST_CASE(strassen_full_width_prime_arithmetic_and_freivalds_integration) {
  using algorithms::linear_algebra::PrimeFieldMatrix;
  using algorithms::linear_algebra::strassen_matrix_multiply_mod;

  constexpr std::uint64_t prime = 18446744073709551557ULL;
  const PrimeFieldMatrix first{{prime - 1U, 1U}, {2U, prime - 2U}};
  const PrimeFieldMatrix second{{prime - 3U, 4U}, {5U, prime - 6U}};
  const PrimeFieldMatrix expected{{8U, prime - 10U}, {prime - 16U, 20U}};
  const auto result = strassen_matrix_multiply_mod(first, second, prime, 1U);
  REQUIRE_EQ(result.product, expected);

  const auto verification =
      algorithms::randomized::freivalds_randomized_verify_matrix_product(
          first, second, result.product, prime, 0x51A55EULL, 8U);
  REQUIRE(verification.accepted);
  REQUIRE_EQ(verification.trials_executed, 8U);
}

TEST_CASE(strassen_randomized_differential_against_independent_cubic_oracle) {
  using algorithms::linear_algebra::strassen_matrix_multiply_mod;

  constexpr std::uint64_t modulus = 101U;
  std::mt19937_64 random(0x57A55EEDULL);
  for (std::size_t trial = 0U; trial < 420U; ++trial) {
    const std::size_t rows = 1U + static_cast<std::size_t>(random() % 8U);
    const std::size_t shared = 1U + static_cast<std::size_t>(random() % 8U);
    const std::size_t columns = 1U + static_cast<std::size_t>(random() % 8U);
    const std::size_t leaf_size = 1U + static_cast<std::size_t>(random() % 4U);
    const auto first =
        strassen_test_detail::random_matrix(random, rows, shared, modulus);
    const auto second =
        strassen_test_detail::random_matrix(random, shared, columns, modulus);
    const auto expected = strassen_test_detail::cubic_oracle(first, second, modulus);
    const auto actual =
        strassen_matrix_multiply_mod(first, second, modulus, leaf_size);
    REQUIRE_EQ(actual.product, expected);

    const auto verification =
        algorithms::randomized::freivalds_randomized_verify_matrix_product(
            first, second, actual.product, modulus,
            static_cast<std::uint64_t>(trial) + 17U, 4U);
    REQUIRE(verification.accepted);
  }
}
