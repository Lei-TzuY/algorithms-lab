#include "algorithms/streaming/count_min.hpp"
#include "algorithms/streaming/gk_quantiles.hpp"
#include "algorithms/number_theory/modular.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
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
using algorithms::streaming::GreenwaldKhannaSummary;

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

bool gk_value_rank_within(const std::vector<std::int64_t>& stream,
                          std::int64_t value, std::size_t target_rank,
                          std::size_t error) {
  std::vector<std::int64_t> sorted = stream;
  std::sort(sorted.begin(), sorted.end());
  const auto first_it = std::lower_bound(sorted.begin(), sorted.end(), value);
  const auto after_it = std::upper_bound(sorted.begin(), sorted.end(), value);
  if (first_it == after_it) {
    return false;
  }
  const std::size_t first_rank =
      static_cast<std::size_t>(first_it - sorted.begin()) + 1U;
  const std::size_t last_rank =
      static_cast<std::size_t>(after_it - sorted.begin());
  const std::size_t lower = target_rank > error ? target_rank - error : 1U;
  const std::size_t upper =
      target_rank > std::numeric_limits<std::size_t>::max() - error
          ? std::numeric_limits<std::size_t>::max()
          : target_rank + error;
  return first_rank <= upper && last_rank >= lower;
}

void verify_gk_all_ranks(const std::vector<std::int64_t>& stream,
                         std::size_t denominator) {
  GreenwaldKhannaSummary summary(denominator);
  for (const std::int64_t value : stream) {
    summary.insert(value);
    REQUIRE(summary.valid_state());
  }
  REQUIRE_EQ(summary.count(), stream.size());
  if (stream.empty()) {
    REQUIRE(!summary.query_rank(1U).has_value());
    return;
  }
  for (std::size_t rank = 1U; rank <= stream.size(); ++rank) {
    const auto estimate = summary.query_rank(rank);
    REQUIRE(estimate.has_value());
    REQUIRE_EQ(estimate->target_rank, rank);
    REQUIRE_EQ(estimate->rank_error_bound, summary.rank_error_bound());
    REQUIRE(gk_value_rank_within(stream, estimate->value, rank,
                                 estimate->rank_error_bound));
  }
}

void enumerate_gk_small_streams(std::vector<std::int64_t>& stream,
                                std::size_t remaining) {
  if (remaining == 0U) {
    for (const std::size_t denominator : {2U, 3U, 5U, 11U}) {
      verify_gk_all_ranks(stream, denominator);
    }
    return;
  }
  for (const std::int64_t value : std::array<std::int64_t, 3>{-1, 0, 1}) {
    stream.push_back(value);
    enumerate_gk_small_streams(stream, remaining - 1U);
    stream.pop_back();
  }
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

TEST_CASE(gk_quantiles_validation_empty_and_boundary_values) {
  REQUIRE_THROWS_AS(GreenwaldKhannaSummary(0U), std::invalid_argument);
  REQUIRE_THROWS_AS(GreenwaldKhannaSummary(1U), std::invalid_argument);

  GreenwaldKhannaSummary empty(10U);
  REQUIRE_EQ(empty.count(), 0U);
  REQUIRE_EQ(empty.summary_size(), 0U);
  REQUIRE_EQ(empty.rank_error_bound(), 0U);
  REQUIRE(empty.valid_state());
  REQUIRE(!empty.query_rank(1U).has_value());

  GreenwaldKhannaSummary summary(10U);
  const std::vector<std::int64_t> values = {
      std::numeric_limits<std::int64_t>::min(), 0, 0,
      std::numeric_limits<std::int64_t>::max()};
  for (const auto value : values) {
    summary.insert(value);
  }
  REQUIRE(summary.valid_state());
  REQUIRE_EQ(summary.count(), values.size());
  REQUIRE_THROWS_AS(summary.query_rank(0U), std::out_of_range);
  REQUIRE_THROWS_AS(summary.query_rank(values.size() + 1U), std::out_of_range);
  for (std::size_t rank = 1U; rank <= values.size(); ++rank) {
    const auto estimate = summary.query_rank(rank);
    REQUIRE(estimate.has_value());
    REQUIRE(gk_value_rank_within(values, estimate->value, rank,
                                 estimate->rank_error_bound));
  }
}

TEST_CASE(gk_quantiles_small_duplicate_domain_is_exhaustively_rank_valid) {
  std::vector<std::int64_t> stream;
  for (std::size_t length = 0U; length <= 7U; ++length) {
    enumerate_gk_small_streams(stream, length);
  }
}

TEST_CASE(gk_quantiles_randomized_exact_rank_window_differential) {
  std::mt19937_64 rng(0x474B5155414E5449ULL);
  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    const std::size_t denominator =
        2U + static_cast<std::size_t>(rng() % 127U);
    GreenwaldKhannaSummary summary(denominator);
    std::vector<std::int64_t> stream;
    const std::size_t length = 1U + static_cast<std::size_t>(rng() % 512U);
    stream.reserve(length);

    for (std::size_t index = 0U; index < length; ++index) {
      std::int64_t value{};
      switch (rng() % 8U) {
        case 0U:
          value = std::numeric_limits<std::int64_t>::min();
          break;
        case 1U:
          value = std::numeric_limits<std::int64_t>::max();
          break;
        default:
          value = static_cast<std::int64_t>(rng() % 201U) - 100;
          break;
      }
      stream.push_back(value);
      summary.insert(value);
      REQUIRE(summary.valid_state());

      if ((index % 29U) == 0U || index + 1U == length) {
        const std::array<std::size_t, 5> ranks = {
            1U,
            stream.size(),
            (stream.size() + 1U) / 2U,
            1U + static_cast<std::size_t>(rng() % stream.size()),
            1U + static_cast<std::size_t>(rng() % stream.size())};
        for (const std::size_t rank : ranks) {
          const auto estimate = summary.query_rank(rank);
          REQUIRE(estimate.has_value());
          REQUIRE(gk_value_rank_within(
              stream, estimate->value, rank, estimate->rank_error_bound));
        }
      }
    }

    GreenwaldKhannaSummary replay(denominator);
    for (const auto value : stream) {
      replay.insert(value);
    }
    REQUIRE_EQ(replay.tuples(), summary.tuples());
    REQUIRE_EQ(replay.count(), summary.count());
  }
}

TEST_CASE(gk_quantiles_compresses_long_monotone_stream_deterministically) {
  GreenwaldKhannaSummary summary(50U);
  for (std::int64_t value = 0; value < 5000; ++value) {
    summary.insert(value);
  }
  REQUIRE(summary.valid_state());
  REQUIRE(summary.summary_size() < summary.count());
  REQUIRE(summary.summary_size() < 200U);

  GreenwaldKhannaSummary replay(50U);
  for (std::int64_t value = 0; value < 5000; ++value) {
    replay.insert(value);
  }
  REQUIRE_EQ(summary.tuples(), replay.tuples());
}
