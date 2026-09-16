#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace algorithms::number_theory {

struct LatticeVector2i {
  std::int64_t x = 0;
  std::int64_t y = 0;

  friend bool operator==(const LatticeVector2i&, const LatticeVector2i&) = default;
};

struct UnimodularTransform2i {
  std::int64_t first_from_first = 1;
  std::int64_t first_from_second = 0;
  std::int64_t second_from_first = 0;
  std::int64_t second_from_second = 1;

  friend bool operator==(const UnimodularTransform2i&,
                         const UnimodularTransform2i&) = default;
};

struct GaussReducedBasis2i {
  LatticeVector2i first;
  LatticeVector2i second;
  UnimodularTransform2i transform;
  std::size_t size_reductions = 0;
};

inline constexpr std::int64_t kGaussLatticeCoordinateBound = 1'000'000;

namespace detail {

[[nodiscard]] inline std::int64_t checked_add(const std::int64_t left,
                                               const std::int64_t right) {
  constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  if ((right > 0 && left > maximum - right) ||
      (right < 0 && left < minimum - right)) {
    throw std::overflow_error("Gauss lattice reduction arithmetic overflow");
  }
  return left + right;
}

[[nodiscard]] inline std::int64_t checked_subtract(const std::int64_t left,
                                                    const std::int64_t right) {
  constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  if ((right > 0 && left < minimum + right) ||
      (right < 0 && left > maximum + right)) {
    throw std::overflow_error("Gauss lattice reduction arithmetic overflow");
  }
  return left - right;
}

[[nodiscard]] inline std::int64_t checked_multiply(const std::int64_t left,
                                                    const std::int64_t right) {
  constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  if (left == 0 || right == 0) {
    return 0;
  }
  if (left == -1) {
    if (right == minimum) {
      throw std::overflow_error("Gauss lattice reduction arithmetic overflow");
    }
    return -right;
  }
  if (right == -1) {
    if (left == minimum) {
      throw std::overflow_error("Gauss lattice reduction arithmetic overflow");
    }
    return -left;
  }
  if (left > 0) {
    if ((right > 0 && left > maximum / right) ||
        (right < 0 && right < minimum / left)) {
      throw std::overflow_error("Gauss lattice reduction arithmetic overflow");
    }
  } else {
    if ((right > 0 && left < minimum / right) ||
        (right < 0 && left < maximum / right)) {
      throw std::overflow_error("Gauss lattice reduction arithmetic overflow");
    }
  }
  return left * right;
}

[[nodiscard]] inline std::int64_t dot(const LatticeVector2i& left,
                                      const LatticeVector2i& right) {
  return checked_add(checked_multiply(left.x, right.x),
                     checked_multiply(left.y, right.y));
}

[[nodiscard]] inline std::int64_t squared_norm(const LatticeVector2i& value) {
  return dot(value, value);
}

[[nodiscard]] inline std::int64_t determinant(const LatticeVector2i& first,
                                              const LatticeVector2i& second) {
  return checked_subtract(checked_multiply(first.x, second.y),
                          checked_multiply(first.y, second.x));
}

[[nodiscard]] inline std::int64_t nearest_quotient_ties_to_zero(
    const std::int64_t numerator, const std::int64_t positive_denominator) {
  const std::int64_t quotient = numerator / positive_denominator;
  const std::int64_t remainder = numerator % positive_denominator;
  const std::int64_t magnitude = remainder < 0 ? -remainder : remainder;
  const bool strictly_more_than_half =
      magnitude > positive_denominator - magnitude;
  if (!strictly_more_than_half) {
    return quotient;
  }
  return checked_add(quotient, numerator < 0 ? -1 : 1);
}

inline void subtract_multiple(LatticeVector2i& target,
                              const LatticeVector2i& source,
                              const std::int64_t multiple) {
  target.x = checked_subtract(target.x, checked_multiply(multiple, source.x));
  target.y = checked_subtract(target.y, checked_multiply(multiple, source.y));
}

inline void subtract_multiple(std::int64_t& target_first,
                              std::int64_t& target_second,
                              const std::int64_t source_first,
                              const std::int64_t source_second,
                              const std::int64_t multiple) {
  target_first =
      checked_subtract(target_first, checked_multiply(multiple, source_first));
  target_second =
      checked_subtract(target_second, checked_multiply(multiple, source_second));
}

[[nodiscard]] inline bool canonical_negative(const LatticeVector2i& value) {
  return value.x < 0 || (value.x == 0 && value.y < 0);
}

inline void normalize_sign(LatticeVector2i& value, std::int64_t& coefficient_first,
                           std::int64_t& coefficient_second) {
  if (!canonical_negative(value)) {
    return;
  }
  value.x = checked_multiply(-1, value.x);
  value.y = checked_multiply(-1, value.y);
  coefficient_first = checked_multiply(-1, coefficient_first);
  coefficient_second = checked_multiply(-1, coefficient_second);
}

inline void validate_input_vector(const LatticeVector2i& value) {
  if (value.x < -kGaussLatticeCoordinateBound ||
      value.x > kGaussLatticeCoordinateBound ||
      value.y < -kGaussLatticeCoordinateBound ||
      value.y > kGaussLatticeCoordinateBound) {
    throw std::out_of_range("Gauss lattice basis coordinate exceeds exact bound");
  }
}

}  // namespace detail

[[nodiscard]] inline GaussReducedBasis2i gauss_reduce_lattice_basis_2d(
    const LatticeVector2i input_first, const LatticeVector2i input_second) {
  detail::validate_input_vector(input_first);
  detail::validate_input_vector(input_second);
  if (detail::determinant(input_first, input_second) == 0) {
    throw std::invalid_argument("Gauss lattice reduction requires an independent 2D basis");
  }

  GaussReducedBasis2i result{input_first, input_second, {}, 0};

  while (true) {
    const std::int64_t first_norm = detail::squared_norm(result.first);
    const std::int64_t second_norm = detail::squared_norm(result.second);
    if (second_norm < first_norm) {
      std::swap(result.first, result.second);
      std::swap(result.transform.first_from_first,
                result.transform.second_from_first);
      std::swap(result.transform.first_from_second,
                result.transform.second_from_second);
      continue;
    }

    const std::int64_t projection = detail::dot(result.first, result.second);
    const std::int64_t multiple =
        detail::nearest_quotient_ties_to_zero(projection, first_norm);
    if (multiple == 0) {
      break;
    }

    detail::subtract_multiple(result.second, result.first, multiple);
    detail::subtract_multiple(result.transform.second_from_first,
                              result.transform.second_from_second,
                              result.transform.first_from_first,
                              result.transform.first_from_second, multiple);
    ++result.size_reductions;
  }

  detail::normalize_sign(result.first, result.transform.first_from_first,
                         result.transform.first_from_second);
  detail::normalize_sign(result.second, result.transform.second_from_first,
                         result.transform.second_from_second);

  if (detail::squared_norm(result.first) == detail::squared_norm(result.second) &&
      std::pair{result.second.x, result.second.y} <
          std::pair{result.first.x, result.first.y}) {
    std::swap(result.first, result.second);
    std::swap(result.transform.first_from_first,
              result.transform.second_from_first);
    std::swap(result.transform.first_from_second,
              result.transform.second_from_second);
  }

  return result;
}

}  // namespace algorithms::number_theory
