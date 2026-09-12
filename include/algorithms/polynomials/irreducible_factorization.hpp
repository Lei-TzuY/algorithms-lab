#pragma once

#include "algorithms/polynomials/equal_degree_factorization.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::polynomials {

struct IrreduciblePolynomialFactor {
  PrimeFieldPolynomial factor;
  std::uint64_t multiplicity;

  friend bool operator==(const IrreduciblePolynomialFactor&,
                         const IrreduciblePolynomialFactor&) = default;
};

struct IrreduciblePolynomialFactorization {
  std::uint64_t unit;
  std::vector<IrreduciblePolynomialFactor> factors;
  std::uint64_t seed;
  std::size_t trials_executed;
  bool complete;
};

namespace irreducible_factorization_detail {

[[nodiscard]] inline std::uint64_t next_subseed(std::uint64_t& state) noexcept {
  // SplitMix64: fixed arithmetic gives a repository-defined, replayable mapping
  // from the caller seed to one deterministic seed per canonical DDF group.
  state += 0x9E3779B97F4A7C15ULL;
  std::uint64_t mixed = state;
  mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBULL;
  return mixed ^ (mixed >> 31U);
}

inline void add_trials(std::size_t& total, std::size_t increment) {
  if (increment > std::numeric_limits<std::size_t>::max() - total) {
    throw std::length_error("irreducible-factorization trial count overflows size_t");
  }
  total += increment;
}

}  // namespace irreducible_factorization_detail

[[nodiscard]] inline IrreduciblePolynomialFactorization
polynomial_irreducible_factorization_mod(
    PrimeFieldPolynomial polynomial, std::uint64_t prime, std::uint64_t seed,
    std::size_t max_trials_per_group) {
  SquareFreeFactorization square_free =
      polynomial_square_free_factorization_mod(std::move(polynomial), prime);

  std::vector<IrreduciblePolynomialFactor> factors;
  std::size_t trials_executed = 0U;
  std::uint64_t seed_state = seed;

  for (const SquareFreeFactor& layer : square_free.factors) {
    DistinctDegreeFactorization grouped =
        polynomial_distinct_degree_factorization_mod(layer.factor, prime);
    if (grouped.unit != 1U) {
      throw std::logic_error(
          "monic square-free layer acquired a non-unit DDF scale");
    }

    for (const DistinctDegreeFactor& group : grouped.factors) {
      const std::uint64_t group_seed =
          irreducible_factorization_detail::next_subseed(seed_state);
      EqualDegreeFactorization split =
          polynomial_equal_degree_factorization_mod(
              group.factor, group.irreducible_degree, prime, group_seed,
              max_trials_per_group);
      irreducible_factorization_detail::add_trials(trials_executed,
                                                   split.trials_executed);
      if (!split.complete) {
        return IrreduciblePolynomialFactorization{
            square_free.unit, {}, seed, trials_executed, false};
      }
      if (split.unit != 1U) {
        throw std::logic_error(
            "monic distinct-degree group acquired a non-unit EDF scale");
      }
      for (PrimeFieldPolynomial& irreducible : split.factors) {
        factors.push_back(IrreduciblePolynomialFactor{
            std::move(irreducible), layer.multiplicity});
      }
    }
  }

  std::sort(factors.begin(), factors.end(),
            [](const IrreduciblePolynomialFactor& first,
               const IrreduciblePolynomialFactor& second) {
              if (first.factor != second.factor) {
                return first.factor < second.factor;
              }
              return first.multiplicity < second.multiplicity;
            });

  return IrreduciblePolynomialFactorization{
      square_free.unit, std::move(factors), seed, trials_executed, true};
}

}  // namespace algorithms::polynomials
