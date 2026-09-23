#include "algorithms/numerical/brent_root.hpp"
#include "test_framework.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

using algorithms::numerical::BrentRootStatus;
using algorithms::numerical::brent_root;

namespace brent_root_test_detail {

bool close(double left, double right, double tolerance = 1e-10) {
  return std::abs(left - right) <=
         tolerance * (1.0 + std::abs(right));
}

template <class Function>
double bisection_reference(Function&& function,
                           double lower,
                           double upper,
                           std::size_t iterations = 160U) {
  double f_lower = function(lower);
  if (f_lower == 0.0) {
    return lower;
  }
  const double f_upper = function(upper);
  if (f_upper == 0.0) {
    return upper;
  }
  REQUIRE(std::signbit(f_lower) != std::signbit(f_upper));

  for (std::size_t iteration = 0U;
       iteration < iterations; ++iteration) {
    const double middle = std::midpoint(lower, upper);
    const double f_middle = function(middle);
    if (f_middle == 0.0) {
      return middle;
    }
    if (std::signbit(f_lower) != std::signbit(f_middle)) {
      upper = middle;
    } else {
      lower = middle;
      f_lower = f_middle;
    }
  }
  return std::midpoint(lower, upper);
}

template <class Function>
void require_bracket(Function&& function,
                     double lower,
                     double upper) {
  REQUIRE(lower <= upper);
  const double f_lower = function(lower);
  const double f_upper = function(upper);
  REQUIRE(f_lower == 0.0 || f_upper == 0.0 ||
          std::signbit(f_lower) != std::signbit(f_upper));
}

}  // namespace brent_root_test_detail

TEST_CASE(brent_root_known_roots_and_endpoint_semantics) {
  const auto square = [](double value) {
    return value * value - 2.0;
  };
  const auto square_result =
      brent_root(square, 0.0, 2.0, 1e-13, 1e-13, 128U);
  REQUIRE(square_result.status == BrentRootStatus::converged);
  REQUIRE(brent_root_test_detail::close(
      square_result.root, std::sqrt(2.0), 2e-12));
  REQUIRE(std::abs(square_result.value) <= 1e-11);
  REQUIRE(square_result.evaluations >= 2U);
  brent_root_test_detail::require_bracket(
      square,
      square_result.lower_bound,
      square_result.upper_bound);

  const auto endpoint = [](double value) {
    return value - 3.0;
  };
  const auto lower_root =
      brent_root(endpoint, 3.0, 9.0);
  REQUIRE(lower_root.status == BrentRootStatus::converged);
  REQUIRE(lower_root.root == 3.0);
  REQUIRE(lower_root.value == 0.0);
  REQUIRE(lower_root.iterations == 0U);
  REQUIRE(lower_root.evaluations == 2U);
  REQUIRE(lower_root.lower_bound == 3.0);
  REQUIRE(lower_root.upper_bound == 3.0);

  const auto upper_root =
      brent_root(endpoint, -4.0, 3.0);
  REQUIRE(upper_root.status == BrentRootStatus::converged);
  REQUIRE(upper_root.root == 3.0);
  REQUIRE(upper_root.iterations == 0U);
}

TEST_CASE(brent_root_transcendental_and_flat_odd_root) {
  const auto fixed_point = [](double value) {
    return std::cos(value) - value;
  };
  const auto fixed =
      brent_root(fixed_point, 0.0, 1.0, 1e-13, 1e-13, 128U);
  REQUIRE(fixed.status == BrentRootStatus::converged);
  REQUIRE(brent_root_test_detail::close(
      fixed.root, 0.7390851332151607, 2e-12));
  REQUIRE(std::abs(fixed.value) <= 1e-11);

  const auto flat = [](double value) {
    const double delta = value - 1.25;
    return delta * delta * delta;
  };
  const auto flat_result =
      brent_root(flat, -1.0, 3.0, 1e-12, 1e-12, 256U);
  REQUIRE(flat_result.status == BrentRootStatus::converged);
  REQUIRE(brent_root_test_detail::close(
      flat_result.root, 1.25, 2e-9));
  brent_root_test_detail::require_bracket(
      flat,
      flat_result.lower_bound,
      flat_result.upper_bound);
}

TEST_CASE(brent_root_validation_and_nonfinite_callback) {
  const auto identity = [](double value) {
    return value;
  };
  REQUIRE_THROWS_AS(
      brent_root(identity, 1.0, 1.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      brent_root(identity,
                 -std::numeric_limits<double>::infinity(),
                 1.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      brent_root(identity, -1.0, 1.0, 0.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      brent_root(identity, -1.0, 1.0, 1e-12, -1e-3),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      brent_root(identity, -1.0, 1.0, 1e-12, 1e-12, 0U),
      std::invalid_argument);

  const auto no_root = [](double value) {
    return value * value + 1.0;
  };
  REQUIRE_THROWS_AS(
      brent_root(no_root, -1.0, 1.0),
      std::invalid_argument);

  const auto nonfinite = [](double) {
    return std::numeric_limits<double>::quiet_NaN();
  };
  REQUIRE_THROWS_AS(
      brent_root(nonfinite, -1.0, 1.0),
      std::overflow_error);
}

TEST_CASE(brent_root_iteration_limit_preserves_bracket) {
  const auto cubic = [](double value) {
    return value * value * value - 2.0;
  };
  const auto result =
      brent_root(cubic, 0.0, 2.0, 1e-18, 0.0, 1U);
  REQUIRE(result.status == BrentRootStatus::iteration_limit);
  REQUIRE(result.iterations == 1U);
  REQUIRE(result.evaluations == 3U);
  brent_root_test_detail::require_bracket(
      cubic, result.lower_bound, result.upper_bound);
  REQUIRE(result.root >= result.lower_bound);
  REQUIRE(result.root <= result.upper_bound);
}

TEST_CASE(brent_root_random_monotone_cubic_differential) {
  std::mt19937_64 random(0xB2E17D3EULL);

  for (std::size_t trial = 0U; trial < 1000U; ++trial) {
    const double root =
        (static_cast<double>(
             static_cast<std::int64_t>(random() % 20001U) - 10000) /
         1000.0);
    const double shape =
        0.25 + static_cast<double>(random() % 2000U) / 500.0;
    const double width =
        0.5 + static_cast<double>(random() % 3000U) / 1000.0;

    const auto function = [root, shape](double value) {
      const double delta = value - root;
      return delta * (delta * delta + shape);
    };

    const double lower = root - width;
    const double upper = root + width;
    const double oracle =
        brent_root_test_detail::bisection_reference(
            function, lower, upper);

    const auto result =
        brent_root(function, lower, upper, 1e-12, 1e-12, 128U);

    REQUIRE(result.status == BrentRootStatus::converged);
    REQUIRE(brent_root_test_detail::close(
        result.root, oracle, 2e-9));
    REQUIRE(std::abs(result.value) <= 1e-8);
    brent_root_test_detail::require_bracket(
        function, result.lower_bound, result.upper_bound);
  }
}
