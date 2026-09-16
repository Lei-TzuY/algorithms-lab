#pragma once

#include "algorithms/combinatorial/robinson_schensted.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <span>
#include <vector>

namespace {

using algorithms::combinatorial::RobinsonSchenstedResult;
using algorithms::combinatorial::StandardTableau;

bool independent_standard_tableau(const StandardTableau& tableau,
                                  std::size_t expected_cells) {
  std::size_t cells = 0;
  for (const auto& row : tableau) {
    cells += row.size();
  }
  if (cells != expected_cells) {
    return false;
  }
  if (!tableau.empty() && tableau.front().empty()) {
    return false;
  }
  for (std::size_t row = 1; row < tableau.size(); ++row) {
    if (tableau[row].empty() || tableau[row].size() > tableau[row - 1].size()) {
      return false;
    }
  }

  std::vector<bool> seen(expected_cells, false);
  for (std::size_t row = 0; row < tableau.size(); ++row) {
    for (std::size_t column = 0; column < tableau[row].size(); ++column) {
      const std::size_t value = tableau[row][column];
      if (value >= expected_cells || seen[value]) {
        return false;
      }
      seen[value] = true;
      if (column != 0 && tableau[row][column - 1] >= value) {
        return false;
      }
      if (row != 0 && column < tableau[row - 1].size() &&
          tableau[row - 1][column] >= value) {
        return false;
      }
    }
  }
  return true;
}

std::size_t independent_lis_length(std::span<const std::size_t> values) {
  if (values.empty()) {
    return 0;
  }
  std::vector<std::size_t> best(values.size(), 1);
  std::size_t answer = 1;
  for (std::size_t i = 0; i < values.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      if (values[j] < values[i]) {
        best[i] = std::max(best[i], best[j] + 1);
      }
    }
    answer = std::max(answer, best[i]);
  }
  return answer;
}

std::size_t independent_lds_length(std::span<const std::size_t> values) {
  if (values.empty()) {
    return 0;
  }
  std::vector<std::size_t> best(values.size(), 1);
  std::size_t answer = 1;
  for (std::size_t i = 0; i < values.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      if (values[j] > values[i]) {
        best[i] = std::max(best[i], best[j] + 1);
      }
    }
    answer = std::max(answer, best[i]);
  }
  return answer;
}

void verify_correspondence(const std::vector<std::size_t>& permutation) {
  const RobinsonSchenstedResult result =
      algorithms::combinatorial::robinson_schensted_permutation(permutation);
  REQUIRE(algorithms::combinatorial::valid_robinson_schensted_result(result));
  REQUIRE(independent_standard_tableau(result.insertion_tableau,
                                       permutation.size()));
  REQUIRE(independent_standard_tableau(result.recording_tableau,
                                       permutation.size()));
  REQUIRE_EQ(result.insertion_tableau.size(), result.recording_tableau.size());
  for (std::size_t row = 0; row < result.insertion_tableau.size(); ++row) {
    REQUIRE_EQ(result.insertion_tableau[row].size(),
               result.recording_tableau[row].size());
  }
  REQUIRE_EQ(algorithms::combinatorial::inverse_robinson_schensted_permutation(
                 result),
             permutation);

  const std::size_t first_row = result.insertion_tableau.empty()
                                    ? 0
                                    : result.insertion_tableau.front().size();
  REQUIRE_EQ(first_row, independent_lis_length(permutation));
  REQUIRE_EQ(result.insertion_tableau.size(), independent_lds_length(permutation));
}

}  // namespace

TEST_CASE(robinson_schensted_known_shapes_and_validation) {
  using algorithms::combinatorial::inverse_robinson_schensted_permutation;
  using algorithms::combinatorial::robinson_schensted_permutation;
  using algorithms::combinatorial::valid_robinson_schensted_result;

  const auto empty =
      robinson_schensted_permutation(std::span<const std::size_t>{});
  REQUIRE(empty.insertion_tableau.empty());
  REQUIRE(empty.recording_tableau.empty());
  REQUIRE(valid_robinson_schensted_result(empty));
  REQUIRE(inverse_robinson_schensted_permutation(empty).empty());

  const std::vector<std::size_t> identity{0, 1, 2, 3, 4};
  const auto identity_result = robinson_schensted_permutation(identity);
  REQUIRE_EQ(identity_result.insertion_tableau,
             StandardTableau({{0, 1, 2, 3, 4}}));
  REQUIRE_EQ(identity_result.recording_tableau,
             StandardTableau({{0, 1, 2, 3, 4}}));

  const std::vector<std::size_t> reverse{3, 2, 1, 0};
  const auto reverse_result = robinson_schensted_permutation(reverse);
  REQUIRE_EQ(reverse_result.insertion_tableau,
             StandardTableau({{0}, {1}, {2}, {3}}));
  REQUIRE_EQ(reverse_result.recording_tableau,
             StandardTableau({{0}, {1}, {2}, {3}}));

  const std::vector<std::size_t> mixed{2, 0, 1};
  const auto mixed_result = robinson_schensted_permutation(mixed);
  REQUIRE_EQ(mixed_result.insertion_tableau, StandardTableau({{0, 1}, {2}}));
  REQUIRE_EQ(mixed_result.recording_tableau, StandardTableau({{0, 2}, {1}}));
  REQUIRE_EQ(inverse_robinson_schensted_permutation(mixed_result), mixed);

  const std::vector<std::size_t> duplicate{0, 1, 1};
  const std::vector<std::size_t> out_of_range{0, 1, 3};
  REQUIRE_THROWS_AS(robinson_schensted_permutation(duplicate),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(robinson_schensted_permutation(out_of_range),
                    std::invalid_argument);

  RobinsonSchenstedResult malformed_shape{{{0, 1}, {2}}, {{0}, {1, 2}}};
  REQUIRE(!valid_robinson_schensted_result(malformed_shape));
  REQUIRE_THROWS_AS(inverse_robinson_schensted_permutation(malformed_shape),
                    std::invalid_argument);

  RobinsonSchenstedResult malformed_order{{{1, 0}}, {{0, 1}}};
  REQUIRE(!valid_robinson_schensted_result(malformed_order));
}

TEST_CASE(robinson_schensted_exhaustive_small_permutations) {
  verify_correspondence({});
  for (std::size_t n = 1; n <= 7; ++n) {
    std::vector<std::size_t> permutation(n);
    std::iota(permutation.begin(), permutation.end(), std::size_t{0});
    do {
      verify_correspondence(permutation);
    } while (std::next_permutation(permutation.begin(), permutation.end()));
  }
}

TEST_CASE(robinson_schensted_randomized_shape_and_inverse_differential) {
  std::mt19937_64 rng(0x52534B5F434F5252ULL);
  for (std::size_t trial = 0; trial < 1200; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 65U);
    std::vector<std::size_t> permutation(n);
    std::iota(permutation.begin(), permutation.end(), std::size_t{0});
    std::shuffle(permutation.begin(), permutation.end(), rng);
    verify_correspondence(permutation);

    const auto first = algorithms::combinatorial::robinson_schensted_permutation(
        permutation);
    const auto second = algorithms::combinatorial::robinson_schensted_permutation(
        permutation);
    REQUIRE_EQ(first.insertion_tableau, second.insertion_tableau);
    REQUIRE_EQ(first.recording_tableau, second.recording_tableau);
  }
}
