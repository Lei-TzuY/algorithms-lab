#pragma once

#include "algorithms/polynomials/distinct_degree_factorization.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::polynomials {

struct EqualDegreeFactorization {
  std::uint64_t unit;
  std::vector<PrimeFieldPolynomial> factors;
  std::uint64_t seed;
  std::size_t trials_executed;
  bool complete;
};

namespace equal_degree_detail {

[[nodiscard]] inline bool is_one(const PrimeFieldPolynomial& polynomial) {
  return polynomial.size() == 1U && polynomial.front() == 1U;
}

[[nodiscard]] inline std::uint64_t sample_below(std::mt19937_64& generator,
                                                std::uint64_t bound) {
  if (bound == 0U) {
    throw std::invalid_argument("random-sampling bound must be non-zero");
  }
  const std::uint64_t threshold = (std::uint64_t{0} - bound) % bound;
  for (;;) {
    const std::uint64_t candidate = generator();
    if (candidate >= threshold) {
      return candidate % bound;
    }
  }
}

[[nodiscard]] inline PrimeFieldPolynomial random_residue(
    std::size_t coefficient_count, std::uint64_t prime,
    std::mt19937_64& generator) {
  PrimeFieldPolynomial polynomial(coefficient_count, 0U);
  for (std::uint64_t& coefficient : polynomial) {
    coefficient = sample_below(generator, prime);
  }
  detail::trim(polynomial);
  return polynomial;
}

[[nodiscard]] inline PrimeFieldPolynomial odd_character_splitter(
    PrimeFieldPolynomial sample, std::size_t irreducible_degree,
    const PrimeFieldPolynomial& modulus, std::uint64_t prime) {
  PrimeFieldPolynomial product{1U};
  PrimeFieldPolynomial frobenius = distinct_degree_detail::remainder(
      std::move(sample), modulus, prime);
  const std::uint64_t half_base = (prime - 1U) / 2U;

  // (p^d - 1)/2 = ((p-1)/2) * (1 + p + ... + p^(d-1)).
  // Evaluate the character power without materializing p^d.
  for (std::size_t step = 0U; step < irreducible_degree; ++step) {
    const PrimeFieldPolynomial component = distinct_degree_detail::power_reduced(
        frobenius, half_base, modulus, prime);
    product = distinct_degree_detail::multiply_reduced(product, component,
                                                        modulus, prime);
    if (step + 1U < irreducible_degree) {
      frobenius = distinct_degree_detail::power_reduced(
          std::move(frobenius), prime, modulus, prime);
    }
  }
  return detail::subtract(product, PrimeFieldPolynomial{1U}, prime);
}

[[nodiscard]] inline PrimeFieldPolynomial binary_trace_splitter(
    PrimeFieldPolynomial sample, std::size_t irreducible_degree,
    const PrimeFieldPolynomial& modulus) {
  PrimeFieldPolynomial trace;
  PrimeFieldPolynomial frobenius = distinct_degree_detail::remainder(
      std::move(sample), modulus, 2U);
  for (std::size_t step = 0U; step < irreducible_degree; ++step) {
    // Addition and subtraction coincide in characteristic two.
    trace = detail::subtract(trace, frobenius, 2U);
    if (step + 1U < irreducible_degree) {
      frobenius = distinct_degree_detail::power_reduced(
          std::move(frobenius), 2U, modulus, 2U);
    }
  }
  return trace;
}

[[nodiscard]] inline PrimeFieldPolynomial exact_quotient(
    const PrimeFieldPolynomial& dividend, const PrimeFieldPolynomial& divisor,
    std::uint64_t prime) {
  auto division = polynomial_divide_mod(dividend, divisor, prime);
  if (!division.remainder.empty()) {
    throw std::logic_error(
        "equal-degree factorization encountered a non-exact split");
  }
  return std::move(division.quotient);
}

}  // namespace equal_degree_detail

[[nodiscard]] inline EqualDegreeFactorization
polynomial_equal_degree_factorization_mod(
    PrimeFieldPolynomial polynomial, std::size_t irreducible_degree,
    std::uint64_t prime, std::uint64_t seed, std::size_t max_trials) {
  if (irreducible_degree == 0U) {
    throw std::invalid_argument(
        "equal-degree factorization degree must be positive");
  }

  // DDF is also the input validator: it checks the prime field, canonical
  // coefficients, nonzero/square-free input, and confirms that every
  // irreducible factor belongs to exactly one requested degree group.
  DistinctDegreeFactorization grouping =
      polynomial_distinct_degree_factorization_mod(std::move(polynomial), prime);
  if (grouping.factors.empty()) {
    return EqualDegreeFactorization{grouping.unit, {}, seed, 0U, true};
  }
  if (grouping.factors.size() != 1U ||
      grouping.factors.front().irreducible_degree != irreducible_degree) {
    throw std::invalid_argument(
        "equal-degree factorization requires one matching DDF group");
  }

  PrimeFieldPolynomial initial = std::move(grouping.factors.front().factor);
  std::vector<PrimeFieldPolynomial> pending;
  pending.push_back(std::move(initial));
  std::vector<PrimeFieldPolynomial> factors;
  std::mt19937_64 generator(seed);
  std::size_t trials_executed = 0U;

  while (!pending.empty()) {
    PrimeFieldPolynomial current = std::move(pending.back());
    pending.pop_back();
    const std::size_t degree = current.size() - 1U;
    if (degree == irreducible_degree) {
      factors.push_back(std::move(current));
      continue;
    }
    if (degree < irreducible_degree || degree % irreducible_degree != 0U) {
      throw std::logic_error(
          "equal-degree split violated degree divisibility");
    }

    bool split = false;
    while (!split) {
      if (trials_executed == max_trials) {
        return EqualDegreeFactorization{grouping.unit, {}, seed,
                                        trials_executed, false};
      }
      ++trials_executed;
      PrimeFieldPolynomial sample = equal_degree_detail::random_residue(
          degree, prime, generator);
      PrimeFieldPolynomial splitter =
          prime == 2U
              ? equal_degree_detail::binary_trace_splitter(
                    std::move(sample), irreducible_degree, current)
              : equal_degree_detail::odd_character_splitter(
                    std::move(sample), irreducible_degree, current, prime);
      PrimeFieldPolynomial left =
          polynomial_gcd_mod(current, splitter, prime);
      if (equal_degree_detail::is_one(left) || left == current) {
        continue;
      }
      PrimeFieldPolynomial right = equal_degree_detail::exact_quotient(
          current, left, prime);
      if (left.size() <= 1U || right.size() <= 1U ||
          (left.size() - 1U) % irreducible_degree != 0U ||
          (right.size() - 1U) % irreducible_degree != 0U) {
        throw std::logic_error(
            "equal-degree factorization produced invalid split");
      }
      pending.push_back(std::move(right));
      pending.push_back(std::move(left));
      split = true;
    }
  }

  std::sort(factors.begin(), factors.end());
  return EqualDegreeFactorization{grouping.unit, std::move(factors), seed,
                                  trials_executed, true};
}

}  // namespace algorithms::polynomials
