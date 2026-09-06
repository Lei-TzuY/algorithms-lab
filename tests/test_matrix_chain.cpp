#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/dynamic_programming/matrix_chain.hpp"

namespace {

using algorithms::dynamic_programming::MatrixChainResult;

std::uint64_t exhaustive_cost(const std::vector<std::uint64_t>& dimensions,
                              std::size_t first, std::size_t last) {
  if (first == last) {
    return 0U;
  }

  std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
  for (std::size_t split = first; split < last; ++split) {
    const std::uint64_t candidate =
        exhaustive_cost(dimensions, first, split) +
        exhaustive_cost(dimensions, split + 1U, last) +
        dimensions[first] * dimensions[split + 1U] *
            dimensions[last + 1U];
    best = std::min(best, candidate);
  }
  return best;
}

struct ReplayedSubchain {
  std::uint64_t cost{0};
  std::uint64_t rows{0};
  std::uint64_t columns{0};
};

ReplayedSubchain replay_plan(const std::vector<std::uint64_t>& dimensions,
                             const MatrixChainResult& result,
                             std::size_t first, std::size_t last,
                             std::size_t& cursor) {
  if (first == last) {
    return ReplayedSubchain{0U, dimensions[first], dimensions[first + 1U]};
  }

  REQUIRE(cursor < result.splits.size());
  const auto& record = result.splits[cursor];
  ++cursor;
  REQUIRE_EQ(record.first_matrix, first);
  REQUIRE_EQ(record.last_matrix, last);
  REQUIRE(record.split_after >= first);
  REQUIRE(record.split_after < last);

  const auto left = replay_plan(dimensions, result, first,
                                record.split_after, cursor);
  const auto right = replay_plan(dimensions, result, record.split_after + 1U,
                                 last, cursor);
  REQUIRE_EQ(left.columns, right.rows);

  return ReplayedSubchain{
      left.cost + right.cost + left.rows * left.columns * right.columns,
      left.rows, right.columns};
}

void require_replay_matches(const std::vector<std::uint64_t>& dimensions,
                            const MatrixChainResult& result) {
  if (dimensions.size() <= 2U) {
    REQUIRE(result.splits.empty());
    REQUIRE_EQ(result.minimum_scalar_multiplications, std::uint64_t{0});
    return;
  }

  const std::size_t matrix_count = dimensions.size() - 1U;
  std::size_t cursor = 0;
  const auto replayed =
      replay_plan(dimensions, result, 0U, matrix_count - 1U, cursor);
  REQUIRE_EQ(cursor, result.splits.size());
  REQUIRE_EQ(replayed.cost, result.minimum_scalar_multiplications);
  REQUIRE_EQ(replayed.rows, dimensions.front());
  REQUIRE_EQ(replayed.columns, dimensions.back());
}

}  // namespace

TEST_CASE(matrix_chain_handles_empty_single_and_invalid_dimensions) {
  REQUIRE_EQ(algorithms::dynamic_programming::matrix_chain_order({})
                 .minimum_scalar_multiplications,
             std::uint64_t{0});
  REQUIRE_EQ(algorithms::dynamic_programming::matrix_chain_order({7})
                 .minimum_scalar_multiplications,
             std::uint64_t{0});

  const auto single =
      algorithms::dynamic_programming::matrix_chain_order({10, 20});
  REQUIRE_EQ(single.minimum_scalar_multiplications, std::uint64_t{0});
  REQUIRE(single.splits.empty());

  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::matrix_chain_order({10, 0, 20}),
      std::invalid_argument);
}

TEST_CASE(matrix_chain_solves_classic_instance_and_replays_plan) {
  const std::vector<std::uint64_t> dimensions{30, 35, 15, 5, 10, 20, 25};
  const auto result =
      algorithms::dynamic_programming::matrix_chain_order(dimensions);
  REQUIRE_EQ(result.minimum_scalar_multiplications, std::uint64_t{15125});
  REQUIRE_EQ(result.splits.size(), std::size_t{5});
  require_replay_matches(dimensions, result);
}

TEST_CASE(matrix_chain_uses_leftmost_ties_and_skips_overflowing_candidates) {
  const auto tied =
      algorithms::dynamic_programming::matrix_chain_order({2, 2, 2, 2});
  REQUIRE_EQ(tied.minimum_scalar_multiplications, std::uint64_t{16});
  REQUIRE_EQ(tied.splits.size(), std::size_t{2});
  REQUIRE_EQ(tied.splits[0].first_matrix, std::size_t{0});
  REQUIRE_EQ(tied.splits[0].last_matrix, std::size_t{2});
  REQUIRE_EQ(tied.splits[0].split_after, std::size_t{0});
  REQUIRE_EQ(tied.splits[1].first_matrix, std::size_t{1});
  REQUIRE_EQ(tied.splits[1].last_matrix, std::size_t{2});
  REQUIRE_EQ(tied.splits[1].split_after, std::size_t{1});

  const std::uint64_t large =
      std::numeric_limits<std::uint64_t>::max() / 3U;
  const std::vector<std::uint64_t> recoverable{large, 1U, large, 1U};
  const auto recovered =
      algorithms::dynamic_programming::matrix_chain_order(recoverable);
  REQUIRE_EQ(recovered.minimum_scalar_multiplications, large * 2U);
  REQUIRE_EQ(recovered.splits[0].split_after, std::size_t{0});
  require_replay_matches(recoverable, recovered);

  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::matrix_chain_order(
          {std::numeric_limits<std::uint64_t>::max(), 2U, 2U}),
      std::overflow_error);
}

TEST_CASE(matrix_chain_matches_exhaustive_randomized_parenthesization_oracle) {
  std::mt19937_64 rng(0x4D4154524958ULL);
  std::uniform_int_distribution<int> matrix_count_distribution(1, 7);
  std::uniform_int_distribution<int> dimension_distribution(1, 12);

  for (std::size_t trial = 0; trial < 380; ++trial) {
    const std::size_t matrix_count =
        static_cast<std::size_t>(matrix_count_distribution(rng));
    std::vector<std::uint64_t> dimensions;
    dimensions.reserve(matrix_count + 1U);
    for (std::size_t index = 0; index <= matrix_count; ++index) {
      dimensions.push_back(
          static_cast<std::uint64_t>(dimension_distribution(rng)));
    }

    const auto result =
        algorithms::dynamic_programming::matrix_chain_order(dimensions);
    const std::uint64_t oracle =
        exhaustive_cost(dimensions, 0U, matrix_count - 1U);
    REQUIRE_EQ(result.minimum_scalar_multiplications, oracle);
    require_replay_matches(dimensions, result);
  }
}
