#include "algorithms/streaming/count_min.hpp"
#include "algorithms/number_theory/modular.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::streaming::CountMinHashRow;
using algorithms::streaming::CountMinSketch;

std::uint64_t toy_collision_count(std::uint64_t prime, std::uint64_t width,
                                  std::uint64_t first, std::uint64_t second) {
  std::uint64_t collisions = 0U;
  for (std::uint64_t multiplier = 1U; multiplier < prime; ++multiplier) {
    for (std::uint64_t increment = 0U; increment < prime; ++increment) {
      const std::uint64_t left = ((multiplier * first + increment) % prime) % width;
      const std::uint64_t right = ((multiplier * second + increment) % prime) % width;
      if (left == right) ++collisions;
    }
  }
  return collisions;
}

std::uint64_t toy_collision_formula(std::uint64_t prime, std::uint64_t width) {
  const std::uint64_t quotient = prime / width;
  const std::uint64_t remainder = prime % width;
  return remainder * (quotient + 1U) * quotient +
         (width - remainder) * quotient * (quotient - 1U);
}

std::vector<CountMinHashRow> make_rows(std::mt19937_64& rng, std::size_t depth) {
  std::vector<CountMinHashRow> rows;
  rows.reserve(depth);
  const std::uint64_t prime = CountMinSketch::hash_prime();
  for (std::size_t row = 0; row < depth; ++row) {
    rows.push_back(CountMinHashRow{1U + (rng() % (prime - 1U)), rng() % prime});
  }
  return rows;
}
}  // namespace

TEST_CASE(count_min_validation_and_basic_contract) {
  REQUIRE(algorithms::number_theory::is_prime(CountMinSketch::hash_prime()));
  REQUIRE(CountMinSketch::hash_prime() > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));
  REQUIRE_THROWS_AS(CountMinSketch(0U, {{1U, 0U}}), std::invalid_argument);
  REQUIRE_THROWS_AS(CountMinSketch(4U, {}), std::invalid_argument);
  REQUIRE_THROWS_AS(CountMinSketch(4U, {{0U, 0U}}), std::invalid_argument);
  REQUIRE_THROWS_AS(
      CountMinSketch(4U, {{CountMinSketch::hash_prime(), 0U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      CountMinSketch(4U, {{1U, CountMinSketch::hash_prime()}}),
      std::invalid_argument);

  CountMinSketch sketch(17U, {{1U, 0U}, {7U, 11U}, {1234567U, 7654321U}});
  REQUIRE_EQ(sketch.width(), 17U);
  REQUIRE_EQ(sketch.depth(), 3U);
  REQUIRE_EQ(sketch.total_weight(), 0U);
  REQUIRE_EQ(sketch.estimate(9U), 0U);
  REQUIRE(sketch.valid_state());

  sketch.add(9U, 3U);
  sketch.add(9U, 4U);
  sketch.add(4U, 2U);
  sketch.add(4U, 0U);
  REQUIRE_EQ(sketch.total_weight(), 9U);
  REQUIRE(sketch.estimate(9U) >= 7U);
  REQUIRE(sketch.estimate(4U) >= 2U);
  REQUIRE(sketch.valid_state());
}

TEST_CASE(count_min_update_overflow_is_transactional) {
  CountMinSketch sketch(1U, {{1U, 0U}, {2U, 1U}});
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  sketch.add(7U, maximum);
  REQUIRE_EQ(sketch.estimate(7U), maximum);
  REQUIRE_EQ(sketch.total_weight(), maximum);
  REQUIRE_THROWS_AS(sketch.add(7U, 1U), std::overflow_error);
  REQUIRE_EQ(sketch.estimate(7U), maximum);
  REQUIRE_EQ(sketch.total_weight(), maximum);
  REQUIRE(sketch.valid_state());
}

TEST_CASE(count_min_affine_family_collision_count_matches_formula) {
  constexpr std::uint64_t prime = 17U;
  for (const std::uint64_t width : {1U, 2U, 4U, 5U, 8U, 17U}) {
    const std::uint64_t expected = toy_collision_formula(prime, width);
    for (std::uint64_t first = 0U; first < prime; ++first) {
      for (std::uint64_t second = first + 1U; second < prime; ++second) {
        REQUIRE_EQ(toy_collision_count(prime, width, first, second), expected);
      }
    }
  }
}

TEST_CASE(count_min_randomized_differential_never_underestimates) {
  std::mt19937_64 rng(0xC0A17A11ULL);
  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t width = 1U + static_cast<std::size_t>(rng() % 31U);
    const std::size_t depth = 1U + static_cast<std::size_t>(rng() % 6U);
    CountMinSketch sketch(width, make_rows(rng, depth));
    std::map<std::uint32_t, std::uint64_t> exact;
    for (std::size_t update = 0; update < 200U; ++update) {
      const auto item = static_cast<std::uint32_t>(rng() % 96U);
      const std::uint64_t amount = 1U + (rng() % 7U);
      sketch.add(item, amount);
      exact[item] += amount;
      REQUIRE(sketch.valid_state());
    }
    for (std::uint32_t item = 0U; item < 112U; ++item) {
      const std::uint64_t estimate = sketch.estimate(item);
      REQUIRE(estimate >= exact[item]);
      REQUIRE(estimate <= sketch.total_weight());
    }
  }
}
