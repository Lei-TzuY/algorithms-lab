#pragma once

#include "algorithms/randomized/exact_alias_sampler.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::randomized::ExactAliasCell;
using algorithms::randomized::ExactAliasSampler;

namespace exact_alias_sampler_test_detail {

using Wide = unsigned __int128;

inline std::vector<Wide> reconstruct_mass(
    const ExactAliasSampler& sampler) {
  std::vector<Wide> mass(sampler.size(), Wide{0});
  const Wide total =
      static_cast<Wide>(sampler.total_weight());

  for (std::size_t column = 0U;
       column < sampler.size(); ++column) {
    const ExactAliasCell cell = sampler.cell(column);
    REQUIRE(cell.alias < sampler.size());
    REQUIRE(cell.threshold <= sampler.total_weight());

    mass[column] += static_cast<Wide>(cell.threshold);
    mass[cell.alias] +=
        total - static_cast<Wide>(cell.threshold);
  }
  return mass;
}

inline void require_exact_mass(
    const ExactAliasSampler& sampler) {
  const std::vector<Wide> mass =
      reconstruct_mass(sampler);
  const Wide count =
      static_cast<Wide>(sampler.size());

  REQUIRE(sampler.valid_distribution());
  for (std::size_t index = 0U;
       index < sampler.size(); ++index) {
    REQUIRE(
        mass[index] ==
        count * static_cast<Wide>(sampler.weight(index)));
  }
}

inline void require_exhaustive_draw_space(
    const ExactAliasSampler& sampler) {
  std::vector<Wide> observed(
      sampler.size(), Wide{0});

  for (std::size_t column = 0U;
       column < sampler.size(); ++column) {
    for (std::uint64_t threshold = 0U;
         threshold < sampler.total_weight();
         ++threshold) {
      const std::size_t outcome =
          sampler.sample_from_draws(column, threshold);
      REQUIRE(outcome < sampler.size());
      ++observed[outcome];
    }
  }

  const Wide count =
      static_cast<Wide>(sampler.size());
  for (std::size_t index = 0U;
       index < sampler.size(); ++index) {
    REQUIRE(
        observed[index] ==
        count * static_cast<Wide>(sampler.weight(index)));
  }
}

}  // namespace exact_alias_sampler_test_detail

TEST_CASE(exact_alias_sampler_rejects_invalid_weight_sets) {
  REQUIRE_THROWS_AS(
      ExactAliasSampler({}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      ExactAliasSampler({0U, 0U, 0U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      ExactAliasSampler({
          std::numeric_limits<std::uint64_t>::max(),
          1U}),
      std::overflow_error);
}

TEST_CASE(exact_alias_sampler_single_outcome_extreme_weight) {
  using namespace exact_alias_sampler_test_detail;

  const std::uint64_t maximum =
      std::numeric_limits<std::uint64_t>::max();
  const ExactAliasSampler sampler({maximum});

  REQUIRE_EQ(sampler.size(), 1U);
  REQUIRE_EQ(sampler.total_weight(), maximum);
  REQUIRE_EQ(sampler.weight(0U), maximum);
  const ExactAliasCell expected{maximum, 0U};
  REQUIRE(sampler.cell(0U) == expected);
  REQUIRE_EQ(
      sampler.sample_from_draws(0U, 0U), 0U);
  REQUIRE_EQ(
      sampler.sample_from_draws(0U, maximum - 1U), 0U);
  REQUIRE(sampler.valid_distribution());
}

TEST_CASE(exact_alias_sampler_exact_small_distribution) {
  using namespace exact_alias_sampler_test_detail;

  const ExactAliasSampler sampler({1U, 2U, 3U});
  REQUIRE_EQ(sampler.total_weight(), 6U);

  require_exact_mass(sampler);
  require_exhaustive_draw_space(sampler);
}

TEST_CASE(exact_alias_sampler_zero_weights_never_receive_mass) {
  using namespace exact_alias_sampler_test_detail;

  const ExactAliasSampler sampler({0U, 5U, 0U, 1U});
  REQUIRE_EQ(sampler.total_weight(), 6U);

  require_exact_mass(sampler);
  require_exhaustive_draw_space(sampler);

  const std::vector<Wide> mass =
      reconstruct_mass(sampler);
  REQUIRE(mass[0U] == Wide{0});
  REQUIRE(mass[2U] == Wide{0});
}

TEST_CASE(exact_alias_sampler_equal_weights_cover_every_cell) {
  using namespace exact_alias_sampler_test_detail;

  const ExactAliasSampler sampler({7U, 7U, 7U, 7U});
  require_exact_mass(sampler);

  for (std::size_t column = 0U;
       column < sampler.size(); ++column) {
    const ExactAliasCell expected{
        sampler.total_weight(), column};
    REQUIRE(sampler.cell(column) == expected);
  }
}

TEST_CASE(exact_alias_sampler_replay_bounds_are_checked) {
  const ExactAliasSampler sampler({2U, 3U});

  REQUIRE_THROWS_AS(
      sampler.weight(2U), std::out_of_range);
  REQUIRE_THROWS_AS(
      sampler.cell(2U), std::out_of_range);
  REQUIRE_THROWS_AS(
      sampler.sample_from_draws(2U, 0U),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      sampler.sample_from_draws(
          0U, sampler.total_weight()),
      std::out_of_range);
}

TEST_CASE(exact_alias_sampler_random_small_tables_have_exact_mass) {
  using namespace exact_alias_sampler_test_detail;

  std::mt19937_64 random(0xA11A5EEDULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t count =
        1U + static_cast<std::size_t>(random() % 12U);
    std::vector<std::uint64_t> weights(count, 0U);

    std::uint64_t total = 0U;
    for (std::uint64_t& weight : weights) {
      weight = random() % 8U;
      total += weight;
    }
    if (total == 0U) {
      weights[
          static_cast<std::size_t>(random() % count)] = 1U;
    }

    const ExactAliasSampler sampler(weights);
    require_exact_mass(sampler);

    if (sampler.total_weight() <= 48U) {
      require_exhaustive_draw_space(sampler);
    }
  }
}

TEST_CASE(exact_alias_sampler_rng_sampling_is_replayable_and_valid) {
  const ExactAliasSampler sampler({0U, 1U, 9U, 0U, 4U});

  std::mt19937_64 first(0xD157A11A5ULL);
  std::mt19937_64 second(0xD157A11A5ULL);

  for (std::size_t draw = 0U; draw < 4000U; ++draw) {
    const std::size_t a = sampler.sample(first);
    const std::size_t b = sampler.sample(second);

    REQUIRE_EQ(a, b);
    REQUIRE(a < sampler.size());
    REQUIRE(sampler.weight(a) > 0U);
  }
}
