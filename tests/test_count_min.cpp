#include "algorithms/streaming/ams_f2.hpp"
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
using algorithms::streaming::AmsF2FixedHorizonSketch;
using algorithms::streaming::AmsF2SampleState;
using algorithms::streaming::CountMinHashRow;
using algorithms::streaming::CountMinSketch;
using algorithms::streaming::GreenwaldKhannaSummary;
using algorithms::streaming::ams_replayable_sample_positions;

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

std::uint64_t exact_ams_f2(const std::vector<std::uint32_t>& stream) {
  std::map<std::uint32_t, std::uint64_t> frequency;
  for (const std::uint32_t item : stream) {
    ++frequency[item];
  }
  std::uint64_t total = 0U;
  for (const auto& [item, count] : frequency) {
    static_cast<void>(item);
    total += count * count;
  }
  return total;
}

std::vector<std::uint64_t> ams_every_position(std::size_t size) {
  std::vector<std::uint64_t> positions;
  positions.reserve(size);
  for (std::size_t index = 0U; index < size; ++index) {
    positions.push_back(static_cast<std::uint64_t>(index));
  }
  return positions;
}

void feed_ams(AmsF2FixedHorizonSketch& sketch,
              const std::vector<std::uint32_t>& stream) {
  for (const std::uint32_t item : stream) {
    sketch.add(item);
    REQUIRE(sketch.valid_state());
  }
}

void enumerate_ams_binary_streams(std::vector<std::uint32_t>& stream,
                                  std::size_t remaining) {
  if (remaining == 0U) {
    if (stream.empty()) {
      return;
    }
    AmsF2FixedHorizonSketch sketch(
        static_cast<std::uint64_t>(stream.size()),
        ams_every_position(stream.size()));
    feed_ams(sketch, stream);
    REQUIRE_EQ(sketch.estimate_f2(),
               static_cast<long double>(exact_ams_f2(stream)));
    return;
  }
  for (const std::uint32_t value : std::array<std::uint32_t, 2>{0U, 1U}) {
    stream.push_back(value);
    enumerate_ams_binary_streams(stream, remaining - 1U);
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

TEST_CASE(ams_f2_validation_and_known_identity) {
  AmsF2FixedHorizonSketch empty(0U, {});
  REQUIRE(empty.complete());
  REQUIRE(empty.valid_state());
  REQUIRE(empty.row_estimates().empty());
  REQUIRE_EQ(empty.estimate_f2(), 0.0L);
  REQUIRE_THROWS_AS(empty.add(7U), std::length_error);
  REQUIRE_THROWS_AS(AmsF2FixedHorizonSketch(4U, {}), std::invalid_argument);
  REQUIRE_THROWS_AS(AmsF2FixedHorizonSketch(4U, {4U}), std::out_of_range);
  REQUIRE(ams_replayable_sample_positions(0U, 0U, 5U).empty());
  REQUIRE_THROWS_AS(ams_replayable_sample_positions(0U, 1U, 5U),
                    std::invalid_argument);

  const std::vector<std::uint32_t> stream{11U, 22U, 11U, 11U, 22U};
  AmsF2FixedHorizonSketch sketch(5U, {0U, 1U, 2U, 3U, 4U});
  REQUIRE_THROWS_AS(sketch.estimate_f2(), std::logic_error);
  feed_ams(sketch, stream);
  REQUIRE_EQ(sketch.row_estimates(),
             (std::vector<long double>{25.0L, 15.0L, 15.0L, 5.0L, 5.0L}));
  REQUIRE_EQ(sketch.estimate_f2(), 13.0L);
  REQUIRE_EQ(exact_ams_f2(stream), std::uint64_t{13});
  REQUIRE_THROWS_AS(sketch.add(11U), std::length_error);
}

TEST_CASE(ams_f2_duplicate_rows_and_seeded_replay) {
  const std::vector<std::uint32_t> stream{4U, 9U, 4U, 9U, 9U};
  AmsF2FixedHorizonSketch sketch(5U, {1U, 1U, 3U});
  feed_ams(sketch, stream);
  REQUIRE_EQ(sketch.samples()[0], sketch.samples()[1]);
  REQUIRE_EQ(sketch.samples()[0], (AmsF2SampleState{1U, 9U, 3U}));
  REQUIRE_EQ(sketch.samples()[2], (AmsF2SampleState{3U, 9U, 2U}));

  const auto first =
      ams_replayable_sample_positions(17U, 64U, 0x123456789abcdef0ULL);
  const auto second =
      ams_replayable_sample_positions(17U, 64U, 0x123456789abcdef0ULL);
  REQUIRE_EQ(first, second);
  REQUIRE_EQ(first.size(), std::size_t{64});
  for (const std::uint64_t position : first) {
    REQUIRE(position < 17U);
  }
}

TEST_CASE(ams_f2_exhaustive_all_positions_equal_exact_second_moment) {
  std::vector<std::uint32_t> stream;
  for (std::size_t length = 1U; length <= 7U; ++length) {
    enumerate_ams_binary_streams(stream, length);
  }
}

TEST_CASE(ams_f2_random_all_positions_differential) {
  std::mt19937_64 rng(0xA45F200DULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t length = 1U + static_cast<std::size_t>(rng() % 30U);
    std::vector<std::uint32_t> stream(length);
    for (std::uint32_t& item : stream) {
      item = static_cast<std::uint32_t>(rng() % 9U);
    }
    AmsF2FixedHorizonSketch sketch(
        static_cast<std::uint64_t>(length), ams_every_position(length));
    feed_ams(sketch, stream);
    REQUIRE_EQ(sketch.estimate_f2(),
               static_cast<long double>(exact_ams_f2(stream)));
  }
}

TEST_CASE(ams_f2_seeded_replay_is_not_an_exactness_contract) {
  const std::vector<std::uint32_t> stream{
      1U, 1U, 1U, 1U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
  const auto positions = ams_replayable_sample_positions(
      static_cast<std::uint64_t>(stream.size()), 3U, 9U);
  AmsF2FixedHorizonSketch first(
      static_cast<std::uint64_t>(stream.size()), positions);
  AmsF2FixedHorizonSketch second(
      static_cast<std::uint64_t>(stream.size()), positions);
  feed_ams(first, stream);
  feed_ams(second, stream);
  REQUIRE_EQ(first.samples(), second.samples());
  REQUIRE_EQ(first.estimate_f2(), second.estimate_f2());
  REQUIRE(first.estimate_f2() !=
          static_cast<long double>(exact_ams_f2(stream)));
}
