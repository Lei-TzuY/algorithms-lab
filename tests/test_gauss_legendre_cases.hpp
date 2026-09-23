#pragma once

#include "algorithms/numerical/gauss_legendre.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

using algorithms::numerical::GaussLegendreRule;
using algorithms::numerical::gauss_legendre_integrate;
using algorithms::numerical::gauss_legendre_rule;
using algorithms::numerical::kMaxGaussLegendreOrder;

namespace gauss_legendre_test_detail {

inline long double integer_power(
    const long double x, const std::size_t exponent) {
  long double result = 1.0L;
  for (std::size_t i = 0U; i < exponent; ++i) {
    result *= x;
  }
  return result;
}

inline long double exact_unit_moment(const std::size_t degree) {
  if ((degree & 1U) != 0U) {
    return 0.0L;
  }
  return 2.0L / static_cast<long double>(degree + 1U);
}

inline long double exact_interval_monomial(
    const long double lower,
    const long double upper,
    const std::size_t degree) {
  const std::size_t power = degree + 1U;
  return (integer_power(upper, power) -
          integer_power(lower, power)) /
         static_cast<long double>(power);
}

inline void require_close(
    const long double actual,
    const long double expected,
    const long double tolerance) {
  const long double scale =
      std::max({1.0L, std::abs(actual), std::abs(expected)});
  REQUIRE(std::abs(actual - expected) <= tolerance * scale);
}

}  // namespace gauss_legendre_test_detail

TEST_CASE(gauss_legendre_rejects_invalid_orders_and_rules) {
  REQUIRE_THROWS_AS(
      gauss_legendre_rule(0U), std::invalid_argument);
  REQUIRE_THROWS_AS(
      gauss_legendre_rule(kMaxGaussLegendreOrder + 1U),
      std::length_error);

  const GaussLegendreRule empty_rule;
  REQUIRE_THROWS_AS(
      gauss_legendre_integrate(
          empty_rule, -1.0L, 1.0L,
          [](const long double x) { return x; }),
      std::invalid_argument);

  GaussLegendreRule mismatched;
  mismatched.nodes = {0.0L};
  REQUIRE_THROWS_AS(
      gauss_legendre_integrate(
          mismatched, -1.0L, 1.0L,
          [](const long double x) { return x; }),
      std::invalid_argument);

  const auto rule = gauss_legendre_rule(2U);
  REQUIRE_THROWS_AS(
      gauss_legendre_integrate(
          rule,
          std::numeric_limits<long double>::infinity(),
          1.0L,
          [](const long double x) { return x; }),
      std::invalid_argument);
}

TEST_CASE(gauss_legendre_known_low_order_rules) {
  using namespace gauss_legendre_test_detail;

  const auto order_one = gauss_legendre_rule(1U);
  REQUIRE_EQ(order_one.nodes.size(), 1U);
  REQUIRE_EQ(order_one.weights.size(), 1U);
  require_close(order_one.nodes[0], 0.0L, 1e-18L);
  require_close(order_one.weights[0], 2.0L, 1e-18L);

  const auto order_two = gauss_legendre_rule(2U);
  const long double expected_node = 1.0L / std::sqrt(3.0L);
  require_close(order_two.nodes[0], -expected_node, 2e-15L);
  require_close(order_two.nodes[1], expected_node, 2e-15L);
  require_close(order_two.weights[0], 1.0L, 2e-15L);
  require_close(order_two.weights[1], 1.0L, 2e-15L);
}

TEST_CASE(gauss_legendre_rules_are_symmetric_positive_and_normalized) {
  using namespace gauss_legendre_test_detail;

  for (std::size_t order = 1U; order <= 32U; ++order) {
    const auto rule = gauss_legendre_rule(order);
    REQUIRE_EQ(rule.nodes.size(), order);
    REQUIRE_EQ(rule.weights.size(), order);

    long double total_weight = 0.0L;
    for (std::size_t index = 0U; index < order; ++index) {
      REQUIRE(rule.nodes[index] > -1.0L);
      REQUIRE(rule.nodes[index] < 1.0L);
      REQUIRE(rule.weights[index] > 0.0L);
      if (index != 0U) {
        REQUIRE(rule.nodes[index - 1U] < rule.nodes[index]);
      }

      const std::size_t mirror = order - 1U - index;
      require_close(
          rule.nodes[index], -rule.nodes[mirror], 5e-14L);
      require_close(
          rule.weights[index], rule.weights[mirror], 5e-14L);
      total_weight += rule.weights[index];
    }
    require_close(total_weight, 2.0L, 5e-14L);
  }
}

TEST_CASE(gauss_legendre_is_exact_for_unit_interval_moments) {
  using namespace gauss_legendre_test_detail;

  for (std::size_t order = 1U; order <= 16U; ++order) {
    const auto rule = gauss_legendre_rule(order);
    for (std::size_t degree = 0U;
         degree <= 2U * order - 1U; ++degree) {
      long double actual = 0.0L;
      for (std::size_t index = 0U;
           index < order; ++index) {
        actual +=
            rule.weights[index] *
            integer_power(rule.nodes[index], degree);
      }
      require_close(
          actual, exact_unit_moment(degree), 2e-12L);
    }
  }
}

TEST_CASE(gauss_legendre_interval_mapping_preserves_polynomial_exactness) {
  using namespace gauss_legendre_test_detail;

  constexpr long double lower = -2.0L;
  constexpr long double upper = 3.0L;

  for (std::size_t order = 1U; order <= 10U; ++order) {
    const auto rule = gauss_legendre_rule(order);
    for (std::size_t degree = 0U;
         degree <= 2U * order - 1U; ++degree) {
      const long double actual =
          gauss_legendre_integrate(
              rule, lower, upper,
              [degree](const long double x) {
                return integer_power(x, degree);
              });
      const long double expected =
          exact_interval_monomial(
              lower, upper, degree);
      require_close(actual, expected, 2e-11L);
    }
  }
}

TEST_CASE(gauss_legendre_handles_reversed_equal_and_nonfinite_integrands) {
  using namespace gauss_legendre_test_detail;

  const auto rule = gauss_legendre_rule(8U);
  const long double forward =
      gauss_legendre_integrate(
          rule, -1.25L, 2.5L,
          [](const long double x) {
            return x * x + 2.0L * x - 3.0L;
          });
  const long double reverse =
      gauss_legendre_integrate(
          rule, 2.5L, -1.25L,
          [](const long double x) {
            return x * x + 2.0L * x - 3.0L;
          });
  require_close(reverse, -forward, 2e-14L);

  bool called = false;
  const long double zero =
      gauss_legendre_integrate(
          rule, 7.0L, 7.0L,
          [&called](const long double) {
            called = true;
            return 1.0L;
          });
  REQUIRE_EQ(zero, 0.0L);
  REQUIRE(!called);

  REQUIRE_THROWS_AS(
      gauss_legendre_integrate(
          rule, -1.0L, 1.0L,
          [](const long double) {
            return std::numeric_limits<long double>::quiet_NaN();
          }),
      std::domain_error);
}

TEST_CASE(gauss_legendre_converges_on_smooth_nonpolynomial_integral) {
  using namespace gauss_legendre_test_detail;

  const long double exact =
      std::exp(1.0L) - std::exp(-1.0L);

  const long double order_four =
      gauss_legendre_integrate(
          4U, -1.0L, 1.0L,
          [](const long double x) { return std::exp(x); });
  const long double order_eight =
      gauss_legendre_integrate(
          8U, -1.0L, 1.0L,
          [](const long double x) { return std::exp(x); });
  const long double order_sixteen =
      gauss_legendre_integrate(
          16U, -1.0L, 1.0L,
          [](const long double x) { return std::exp(x); });

  const long double error_four = std::abs(order_four - exact);
  const long double error_eight = std::abs(order_eight - exact);
  const long double error_sixteen = std::abs(order_sixteen - exact);

  REQUIRE(error_eight < error_four);
  REQUIRE(error_sixteen <= error_eight);
  require_close(order_sixteen, exact, 2e-13L);
}
