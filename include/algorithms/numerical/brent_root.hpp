#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace algorithms::numerical {

enum class BrentRootStatus {
  converged,
  iteration_limit,
};

struct BrentRootResult {
  BrentRootStatus status = BrentRootStatus::converged;
  double root = 0.0;
  double value = 0.0;
  double lower_bound = 0.0;
  double upper_bound = 0.0;
  std::size_t iterations = 0U;
  std::size_t evaluations = 0U;
};

namespace detail {

inline bool brent_opposite_sign(double left, double right) noexcept {
  return left != 0.0 && right != 0.0 &&
         std::signbit(left) != std::signbit(right);
}

inline double brent_checked_tolerance(double point,
                                      double absolute_tolerance,
                                      double relative_tolerance) {
  const double scaled = relative_tolerance * std::abs(point);
  const double tolerance = absolute_tolerance + scaled;
  if (!std::isfinite(tolerance) || tolerance <= 0.0) {
    throw std::overflow_error("Brent tolerance overflowed");
  }
  return tolerance;
}

inline BrentRootResult brent_result(BrentRootStatus status,
                                    double root,
                                    double value,
                                    double first,
                                    double second,
                                    std::size_t iterations,
                                    std::size_t evaluations) {
  return {
      status,
      root,
      value,
      std::min(first, second),
      std::max(first, second),
      iterations,
      evaluations,
  };
}

}  // namespace detail

// Solve f(x)=0 inside a finite sign-changing bracket with the
// Brent-Dekker safeguarded interpolation method.
//
// The solver keeps a valid sign bracket, tries inverse quadratic
// interpolation when three distinct function values are available, otherwise
// tries a secant step, and falls back to a midpoint step whenever the
// interpolation candidate does not satisfy the Brent safeguarding tests.
//
// Semantic precondition: f is continuous on [lower_bound, upper_bound].
// Continuity cannot be validated from finitely many evaluations.
//
// absolute_tolerance must be positive and finite. relative_tolerance must be
// finite and nonnegative. max_iterations must be positive. Non-finite callback
// results are rejected with std::overflow_error.
template <class Function>
[[nodiscard]] BrentRootResult brent_root(
    Function&& function,
    double lower_bound,
    double upper_bound,
    double absolute_tolerance = 1e-12,
    double relative_tolerance = 1e-12,
    std::size_t max_iterations = 128U) {
  if (!std::isfinite(lower_bound) || !std::isfinite(upper_bound) ||
      !(lower_bound < upper_bound)) {
    throw std::invalid_argument(
        "Brent root bracket must be finite and strictly ordered");
  }
  if (!std::isfinite(absolute_tolerance) || absolute_tolerance <= 0.0) {
    throw std::invalid_argument(
        "Brent absolute tolerance must be positive and finite");
  }
  if (!std::isfinite(relative_tolerance) || relative_tolerance < 0.0) {
    throw std::invalid_argument(
        "Brent relative tolerance must be finite and nonnegative");
  }
  if (max_iterations == 0U) {
    throw std::invalid_argument(
        "Brent iteration limit must be positive");
  }

  std::size_t evaluations = 0U;
  const auto evaluate = [&](double point) {
    const double value = function(point);
    ++evaluations;
    if (!std::isfinite(value)) {
      throw std::overflow_error(
          "Brent function evaluation must be finite");
    }
    return value;
  };

  double a = lower_bound;
  double b = upper_bound;
  double fa = evaluate(a);
  double fb = evaluate(b);

  if (fa == 0.0) {
    return detail::brent_result(
        BrentRootStatus::converged, a, fa, a, a, 0U, evaluations);
  }
  if (fb == 0.0) {
    return detail::brent_result(
        BrentRootStatus::converged, b, fb, b, b, 0U, evaluations);
  }
  if (!detail::brent_opposite_sign(fa, fb)) {
    throw std::invalid_argument(
        "Brent root bracket endpoints must have opposite signs");
  }

  // b is always the endpoint with the smaller residual magnitude.
  if (std::abs(fa) < std::abs(fb)) {
    std::swap(a, b);
    std::swap(fa, fb);
  }

  double c = a;
  double fc = fa;
  double d = c;
  bool midpoint_last = true;

  for (std::size_t iteration = 1U;
       iteration <= max_iterations; ++iteration) {
    const double tolerance = detail::brent_checked_tolerance(
        b, absolute_tolerance, relative_tolerance);
    if (fb == 0.0 || std::abs(b - a) <= tolerance) {
      return detail::brent_result(
          BrentRootStatus::converged, b, fb, a, b,
          iteration - 1U, evaluations);
    }

    double candidate = std::numeric_limits<double>::quiet_NaN();

    if (fa != fc && fb != fc && fa != fb) {
      const long double la = static_cast<long double>(a);
      const long double lb = static_cast<long double>(b);
      const long double lc = static_cast<long double>(c);
      const long double lfa = static_cast<long double>(fa);
      const long double lfb = static_cast<long double>(fb);
      const long double lfc = static_cast<long double>(fc);

      const long double first =
          la * lfb * lfc /
          ((lfa - lfb) * (lfa - lfc));
      const long double second =
          lb * lfa * lfc /
          ((lfb - lfa) * (lfb - lfc));
      const long double third =
          lc * lfa * lfb /
          ((lfc - lfa) * (lfc - lfb));
      const long double interpolated = first + second + third;

      if (std::isfinite(interpolated) &&
          interpolated >=
              static_cast<long double>(
                  -std::numeric_limits<double>::max()) &&
          interpolated <=
              static_cast<long double>(
                  std::numeric_limits<double>::max())) {
        candidate = static_cast<double>(interpolated);
      }
    } else if (fb != fa) {
      const long double la = static_cast<long double>(a);
      const long double lb = static_cast<long double>(b);
      const long double lfa = static_cast<long double>(fa);
      const long double lfb = static_cast<long double>(fb);
      const long double interpolated =
          lb - lfb * (lb - la) / (lfb - lfa);

      if (std::isfinite(interpolated) &&
          interpolated >=
              static_cast<long double>(
                  -std::numeric_limits<double>::max()) &&
          interpolated <=
              static_cast<long double>(
                  std::numeric_limits<double>::max())) {
        candidate = static_cast<double>(interpolated);
      }
    }

    const double guarded_edge = 0.75 * a + 0.25 * b;
    const double guard_lower = std::min(guarded_edge, b);
    const double guard_upper = std::max(guarded_edge, b);

    const bool outside_guard =
        !std::isfinite(candidate) ||
        !(candidate > guard_lower && candidate < guard_upper);
    const bool insufficient_midpoint_progress =
        midpoint_last &&
        std::abs(candidate - b) >= 0.5 * std::abs(b - c);
    const bool insufficient_interpolation_progress =
        !midpoint_last &&
        std::abs(candidate - b) >= 0.5 * std::abs(c - d);
    const bool midpoint_history_tiny =
        midpoint_last && std::abs(b - c) < tolerance;
    const bool interpolation_history_tiny =
        !midpoint_last && std::abs(c - d) < tolerance;

    if (outside_guard ||
        insufficient_midpoint_progress ||
        insufficient_interpolation_progress ||
        midpoint_history_tiny ||
        interpolation_history_tiny) {
      candidate = std::midpoint(a, b);
      midpoint_last = true;
    } else {
      midpoint_last = false;
    }

    const double f_candidate = evaluate(candidate);

    d = c;
    c = b;
    fc = fb;

    if (f_candidate == 0.0) {
      return detail::brent_result(
          BrentRootStatus::converged,
          candidate,
          f_candidate,
          candidate,
          candidate,
          iteration,
          evaluations);
    }

    if (detail::brent_opposite_sign(fa, f_candidate)) {
      b = candidate;
      fb = f_candidate;
    } else {
      a = candidate;
      fa = f_candidate;
    }

    if (std::abs(fa) < std::abs(fb)) {
      std::swap(a, b);
      std::swap(fa, fb);
    }

    const double updated_tolerance = detail::brent_checked_tolerance(
        b, absolute_tolerance, relative_tolerance);
    if (std::abs(b - a) <= updated_tolerance) {
      return detail::brent_result(
          BrentRootStatus::converged, b, fb, a, b,
          iteration, evaluations);
    }
  }

  return detail::brent_result(
      BrentRootStatus::iteration_limit,
      b,
      fb,
      a,
      b,
      max_iterations,
      evaluations);
}

}  // namespace algorithms::numerical
