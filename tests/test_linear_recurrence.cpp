#include "algorithms/number_theory/linear_recurrence.hpp"

#include "algorithms/number_theory/modular.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

std::uint64_t add_mod(std::uint64_t a, std::uint64_t b,
                      std::uint64_t modulus) {
  return a >= modulus - b ? a - (modulus - b) : a + b;
}

std::vector<std::uint64_t> generate_sequence(
    std::vector<std::uint64_t> initial,
    const std::vector<std::uint64_t>& coefficients, std::size_t count,
    std::uint64_t modulus) {
  if (coefficients.empty()) {
    initial.resize(count, 0U);
    return initial;
  }
  if (initial.size() < coefficients.size()) {
    throw std::logic_error("test generator needs k initial terms");
  }
  initial.resize(count, 0U);
  for (std::size_t index = coefficients.size(); index < count; ++index) {
    std::uint64_t value = 0U;
    for (std::size_t offset = 0; offset < coefficients.size(); ++offset) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          coefficients[offset], initial[index - 1U - offset], modulus);
      value = add_mod(value, term, modulus);
    }
    initial[index] = value;
  }
  return initial;
}

bool recurrence_fits(const std::vector<std::uint64_t>& sequence,
                     const std::vector<std::uint64_t>& coefficients,
                     std::uint64_t modulus) {
  if (coefficients.empty()) {
    return std::all_of(sequence.begin(), sequence.end(),
                       [](std::uint64_t value) { return value == 0U; });
  }
  for (std::size_t index = coefficients.size(); index < sequence.size(); ++index) {
    std::uint64_t value = 0U;
    for (std::size_t offset = 0; offset < coefficients.size(); ++offset) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          coefficients[offset], sequence[index - 1U - offset], modulus);
      value = add_mod(value, term, modulus);
    }
    if (value != sequence[index]) {
      return false;
    }
  }
  return true;
}

std::size_t exhaustive_minimum_order(const std::vector<std::uint64_t>& sequence,
                                     std::uint64_t modulus) {
  if (std::all_of(sequence.begin(), sequence.end(),
                  [](std::uint64_t value) { return value == 0U; })) {
    return 0U;
  }
  for (std::size_t order = 1U; order <= sequence.size(); ++order) {
    std::uint64_t candidate_count = 1U;
    for (std::size_t i = 0; i < order; ++i) {
      candidate_count *= modulus;
    }
    for (std::uint64_t code = 0U; code < candidate_count; ++code) {
      std::uint64_t remaining = code;
      std::vector<std::uint64_t> coefficients(order, 0U);
      for (std::size_t i = 0; i < order; ++i) {
        coefficients[i] = remaining % modulus;
        remaining /= modulus;
      }
      if (recurrence_fits(sequence, coefficients, modulus)) {
        return order;
      }
    }
  }
  return sequence.size();
}

}  // namespace

TEST_CASE(linear_recurrence_known_sequences_and_validation) {
  using algorithms::number_theory::berlekamp_massey;
  using algorithms::number_theory::linear_recurrence_nth;

  const auto empty = berlekamp_massey(std::vector<std::uint64_t>{}, 101U);
  REQUIRE(empty.coefficients.empty());

  const auto zero =
      berlekamp_massey(std::vector<std::uint64_t>{0U, 0U, 0U, 0U}, 101U);
  REQUIRE(zero.coefficients.empty());
  REQUIRE_EQ(linear_recurrence_nth(std::vector<std::uint64_t>{0U},
                                   zero.coefficients, 1000U, 101U),
             0U);

  const auto fibonacci = berlekamp_massey(
      std::vector<std::uint64_t>{0U, 1U, 1U, 2U, 3U, 5U, 8U, 13U, 21U, 34U},
      101U);
  REQUIRE_EQ(fibonacci.coefficients,
             (std::vector<std::uint64_t>{1U, 1U}));
  REQUIRE_EQ(linear_recurrence_nth(std::vector<std::uint64_t>{0U, 1U},
                                   fibonacci.coefficients, 10U, 101U),
             55U);

  const auto geometric = berlekamp_massey(
      std::vector<std::uint64_t>{3U, 15U, 75U, 72U, 57U, 83U}, 101U);
  REQUIRE_EQ(geometric.coefficients, (std::vector<std::uint64_t>{5U}));

  REQUIRE_THROWS_AS(
      berlekamp_massey(std::vector<std::uint64_t>{1U, 2U}, 15U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      berlekamp_massey(std::vector<std::uint64_t>{1U, 7U}, 7U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      linear_recurrence_nth(std::vector<std::uint64_t>{1U},
                            std::vector<std::uint64_t>{}, 2U, 101U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      linear_recurrence_nth(std::vector<std::uint64_t>{1U},
                            std::vector<std::uint64_t>{}, 0U, 101U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      linear_recurrence_nth(std::vector<std::uint64_t>{1U},
                            std::vector<std::uint64_t>{1U, 1U}, 0U, 101U),
      std::invalid_argument);
}

TEST_CASE(berlekamp_massey_matches_exhaustive_minimum_order) {
  using algorithms::number_theory::berlekamp_massey;
  constexpr std::uint64_t modulus = 5U;
  std::mt19937_64 rng(0xBEEFULL);

  for (std::size_t trial = 0; trial < 240U; ++trial) {
    std::vector<std::uint64_t> sequence(8U, 0U);
    for (std::uint64_t& value : sequence) {
      value = rng() % modulus;
    }
    const auto recurrence = berlekamp_massey(sequence, modulus);
    REQUIRE(recurrence_fits(sequence, recurrence.coefficients, modulus));
    REQUIRE_EQ(recurrence.coefficients.size(),
               exhaustive_minimum_order(sequence, modulus));
  }
}

TEST_CASE(linear_recurrence_fast_extrapolation_matches_direct_rollout) {
  using algorithms::number_theory::berlekamp_massey;
  using algorithms::number_theory::linear_recurrence_nth;
  constexpr std::uint64_t modulus = 1000003U;
  std::mt19937_64 rng(0x1234ABCDULL);

  for (std::size_t trial = 0; trial < 240U; ++trial) {
    const std::size_t order = 1U + static_cast<std::size_t>(rng() % 6U);
    std::vector<std::uint64_t> coefficients(order, 0U);
    std::vector<std::uint64_t> initial(order, 0U);
    for (std::uint64_t& value : coefficients) {
      value = rng() % modulus;
    }
    for (std::uint64_t& value : initial) {
      value = rng() % modulus;
    }

    const auto sequence = generate_sequence(initial, coefficients, 160U, modulus);
    const std::vector<std::uint64_t> observed(sequence.begin(),
                                              sequence.begin() + 40);
    const auto recurrence = berlekamp_massey(observed, modulus);
    REQUIRE(recurrence_fits(observed, recurrence.coefficients, modulus));

    const std::vector<std::uint64_t> minimal_initial(
        sequence.begin(), sequence.begin() +
                              static_cast<std::ptrdiff_t>(recurrence.coefficients.size()));
    for (const std::uint64_t index : {40ULL, 57ULL, 100ULL, 159ULL}) {
      REQUIRE_EQ(linear_recurrence_nth(minimal_initial, recurrence.coefficients,
                                       index, modulus),
                 sequence[static_cast<std::size_t>(index)]);
    }
  }
}
