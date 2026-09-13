#pragma once

#include "algorithms/polynomials/multipoint_evaluation.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace {

std::uint64_t multipoint_horner_oracle(
    const std::vector<std::uint64_t>& coefficients, std::uint64_t point) {
  point %= algorithms::polynomials::kNttModulus;
  std::uint64_t value = 0U;
  for (auto iterator = coefficients.rbegin(); iterator != coefficients.rend();
       ++iterator) {
    value = (value * point + (*iterator % algorithms::polynomials::kNttModulus)) %
            algorithms::polynomials::kNttModulus;
  }
  return value;
}

std::vector<std::uint64_t> multipoint_horner_all(
    const std::vector<std::uint64_t>& coefficients,
    const std::vector<std::uint64_t>& points) {
  std::vector<std::uint64_t> result;
  result.reserve(points.size());
  for (const std::uint64_t point : points) {
    result.push_back(multipoint_horner_oracle(coefficients, point));
  }
  return result;
}

}  // namespace

TEST_CASE(multipoint_evaluation_known_values_and_duplicate_points) {
  const std::vector<std::uint64_t> coefficients{5U, 3U, 2U};
  const std::vector<std::uint64_t> points{0U, 1U, 2U, 1U};
  const auto actual =
      algorithms::polynomials::multipoint_evaluate_mod_998244353(coefficients,
                                                                  points);
  REQUIRE_EQ(actual, std::vector<std::uint64_t>({5U, 10U, 19U, 10U}));
}

TEST_CASE(multipoint_evaluation_empty_zero_and_modular_normalization) {
  const std::vector<std::uint64_t> no_points;
  const std::vector<std::uint64_t> polynomial{1U, 2U, 3U};
  REQUIRE(algorithms::polynomials::multipoint_evaluate_mod_998244353(
              polynomial, no_points)
              .empty());

  const std::vector<std::uint64_t> points{0U, 1U, 7U};
  const std::vector<std::uint64_t> zero_polynomial;
  REQUIRE_EQ(algorithms::polynomials::multipoint_evaluate_mod_998244353(
                 zero_polynomial, points),
             std::vector<std::uint64_t>({0U, 0U, 0U}));

  const std::vector<std::uint64_t> unreduced{
      algorithms::polynomials::kNttModulus + 4U,
      algorithms::polynomials::kNttModulus * 2U + 3U};
  const std::vector<std::uint64_t> unreduced_points{
      algorithms::polynomials::kNttModulus + 2U, 2U};
  REQUIRE_EQ(algorithms::polynomials::multipoint_evaluate_mod_998244353(
                 unreduced, unreduced_points),
             std::vector<std::uint64_t>({10U, 10U}));
}

TEST_CASE(multipoint_evaluation_degree_larger_than_point_count) {
  std::vector<std::uint64_t> coefficients(257U, 0U);
  for (std::size_t index = 0U; index < coefficients.size(); ++index) {
    coefficients[index] =
        (static_cast<std::uint64_t>(index) * 104729U + 17U) %
        algorithms::polynomials::kNttModulus;
  }
  const std::vector<std::uint64_t> points{3U, 5U, 8U, 13U, 21U, 34U, 55U};
  REQUIRE_EQ(algorithms::polynomials::multipoint_evaluate_mod_998244353(
                 coefficients, points),
             multipoint_horner_all(coefficients, points));
}

TEST_CASE(multipoint_evaluation_randomized_differential_against_horner) {
  std::mt19937_64 generator(0x4D554C5449504F49ULL);
  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::size_t coefficient_count =
        static_cast<std::size_t>(generator() % 97U);
    const std::size_t point_count = static_cast<std::size_t>(generator() % 65U);

    std::vector<std::uint64_t> coefficients(coefficient_count, 0U);
    for (auto& coefficient : coefficients) {
      coefficient = generator();
    }
    std::vector<std::uint64_t> points(point_count, 0U);
    for (auto& point : points) {
      point = generator();
    }
    if (points.size() >= 3U && trial % 5U == 0U) {
      points[2U] = points[0U];
    }

    const auto actual =
        algorithms::polynomials::multipoint_evaluate_mod_998244353(coefficients,
                                                                    points);
    const auto expected = multipoint_horner_all(coefficients, points);
    REQUIRE_EQ(actual, expected);
  }
}
