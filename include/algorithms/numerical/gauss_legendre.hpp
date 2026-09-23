#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

inline constexpr std::size_t kMaxGaussLegendreOrder = 256U;

struct GaussLegendreRule {
  std::vector<long double> nodes;
  std::vector<long double> weights;

  friend bool operator==(const GaussLegendreRule&,
                         const GaussLegendreRule&) = default;
};

namespace detail {

[[nodiscard]] inline std::pair<long double, long double>
legendre_value_and_previous(const std::size_t order,
                            const long double x) {
  if (order == 0U) {
    return {1.0L, 0.0L};
  }

  long double previous = 1.0L;
  long double current = x;
  if (order == 1U) {
    return {current, previous};
  }

  for (std::size_t degree = 2U; degree <= order; ++degree) {
    const long double k = static_cast<long double>(degree);
    const long double next =
        ((2.0L * k - 1.0L) * x * current -
         (k - 1.0L) * previous) /
        k;
    previous = current;
    current = next;
  }
  return {current, previous};
}

[[nodiscard]] inline long double legendre_derivative(
    const std::size_t order, const long double x,
    const long double value, const long double previous) {
  const long double denominator = 1.0L - x * x;
  if (!(denominator > 0.0L)) {
    throw std::runtime_error(
        "Gauss-Legendre root escaped the open unit interval");
  }
  return static_cast<long double>(order) *
         (previous - x * value) / denominator;
}

}  // namespace detail

// Construct the deterministic order-n Gauss-Legendre rule on [-1,1].
//
// Nodes are returned in strictly increasing order. Weights are positive and
// symmetric. Orders above kMaxGaussLegendreOrder are intentionally rejected;
// this slice makes numerical guarantees only inside that bounded contract.
[[nodiscard]] inline GaussLegendreRule gauss_legendre_rule(
    const std::size_t order) {
  if (order == 0U) {
    throw std::invalid_argument(
        "Gauss-Legendre quadrature order must be positive");
  }
  if (order > kMaxGaussLegendreOrder) {
    throw std::length_error(
        "Gauss-Legendre quadrature order exceeds supported bound");
  }

  GaussLegendreRule rule;
  rule.nodes.assign(order, 0.0L);
  rule.weights.assign(order, 0.0L);

  const long double pi = std::acos(-1.0L);
  const long double epsilon =
      std::numeric_limits<long double>::epsilon();
  const std::size_t roots_to_solve = (order + 1U) / 2U;

  for (std::size_t root_index = 0U;
       root_index < roots_to_solve; ++root_index) {
    const std::size_t mirror = order - 1U - root_index;
    long double root = 0.0L;

    if (!(order % 2U == 1U && root_index == mirror)) {
      const long double numerator =
          static_cast<long double>(4U * root_index + 3U);
      const long double denominator =
          static_cast<long double>(4U * order + 2U);
      root = std::cos(pi * numerator / denominator);

      bool converged = false;
      for (std::size_t iteration = 0U; iteration < 64U; ++iteration) {
        const auto [value, previous] =
            detail::legendre_value_and_previous(order, root);
        const long double derivative =
            detail::legendre_derivative(
                order, root, value, previous);
        if (!std::isfinite(derivative) || derivative == 0.0L) {
          throw std::runtime_error(
              "Gauss-Legendre Newton derivative is invalid");
        }

        const long double delta = value / derivative;
        const long double next = root - delta;
        if (!std::isfinite(next) || !(next > -1.0L && next < 1.0L)) {
          throw std::runtime_error(
              "Gauss-Legendre Newton iteration left valid domain");
        }

        root = next;
        const long double scale =
            std::max(1.0L, std::abs(root));
        if (std::abs(delta) <= 32.0L * epsilon * scale) {
          converged = true;
          break;
        }
      }
      if (!converged) {
        throw std::runtime_error(
            "Gauss-Legendre Newton iteration did not converge");
      }
    }

    const auto [value, previous] =
        detail::legendre_value_and_previous(order, root);
    const long double derivative =
        detail::legendre_derivative(order, root, value, previous);
    const long double one_minus_square = 1.0L - root * root;
    const long double weight =
        2.0L /
        (one_minus_square * derivative * derivative);

    if (!std::isfinite(weight) || !(weight > 0.0L)) {
      throw std::runtime_error(
          "Gauss-Legendre weight is invalid");
    }

    if (root_index == mirror) {
      rule.nodes[root_index] = 0.0L;
      rule.weights[root_index] = weight;
    } else {
      rule.nodes[root_index] = -root;
      rule.nodes[mirror] = root;
      rule.weights[root_index] = weight;
      rule.weights[mirror] = weight;
    }
  }

  for (std::size_t index = 1U; index < order; ++index) {
    if (!(rule.nodes[index - 1U] < rule.nodes[index])) {
      throw std::logic_error(
          "Gauss-Legendre nodes are not strictly ordered");
    }
  }
  return rule;
}

// Apply a generated Gauss-Legendre rule to a finite interval [lower,upper].
// Reversed bounds are supported and negate the integral. Equal bounds return
// zero without invoking the integrand.
template <typename Function>
[[nodiscard]] inline long double gauss_legendre_integrate(
    const GaussLegendreRule& rule,
    const long double lower,
    const long double upper,
    Function&& function) {
  if (rule.nodes.empty() ||
      rule.nodes.size() != rule.weights.size()) {
    throw std::invalid_argument(
        "Gauss-Legendre rule shape is invalid");
  }
  if (!std::isfinite(lower) || !std::isfinite(upper)) {
    throw std::invalid_argument(
        "Gauss-Legendre integration bounds must be finite");
  }
  if (lower == upper) {
    return 0.0L;
  }

  const long double midpoint = lower / 2.0L + upper / 2.0L;
  const long double half_width = upper / 2.0L - lower / 2.0L;

  long double weighted_sum = 0.0L;
  for (std::size_t index = 0U;
       index < rule.nodes.size(); ++index) {
    const long double x =
        std::fma(half_width, rule.nodes[index], midpoint);
    const long double value =
        static_cast<long double>(function(x));
    if (!std::isfinite(value)) {
      throw std::domain_error(
          "Gauss-Legendre integrand returned a non-finite value");
    }
    weighted_sum += rule.weights[index] * value;
  }

  const long double result = half_width * weighted_sum;
  if (!std::isfinite(result)) {
    throw std::overflow_error(
        "Gauss-Legendre integral is not finite");
  }
  return result;
}

template <typename Function>
[[nodiscard]] inline long double gauss_legendre_integrate(
    const std::size_t order,
    const long double lower,
    const long double upper,
    Function&& function) {
  const GaussLegendreRule rule = gauss_legendre_rule(order);
  return gauss_legendre_integrate(
      rule, lower, upper, std::forward<Function>(function));
}

}  // namespace algorithms::numerical
