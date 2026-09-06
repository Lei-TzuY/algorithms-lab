#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "algorithms/dynamic_programming/longest_increasing_subsequence.hpp"

namespace {

using algorithms::dynamic_programming::LongestIncreasingSubsequenceResult;

void require_valid_witness(const std::vector<std::int64_t>& values,
                           const LongestIncreasingSubsequenceResult& result) {
  for (std::size_t position = 0; position < result.indices.size(); ++position) {
    REQUIRE(result.indices[position] < values.size());
    if (position == 0) {
      continue;
    }
    REQUIRE(result.indices[position - 1] < result.indices[position]);
    REQUIRE(values[result.indices[position - 1]] <
            values[result.indices[position]]);
  }
}

std::size_t exhaustive_lis_length(const std::vector<std::int64_t>& values) {
  REQUIRE(values.size() <= 20);
  const std::uint64_t limit = std::uint64_t{1} << values.size();
  std::size_t best = 0;
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    bool valid = true;
    bool have_previous = false;
    std::int64_t previous = 0;
    std::size_t length = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if ((mask & (std::uint64_t{1} << index)) == 0U) {
        continue;
      }
      if (have_previous && previous >= values[index]) {
        valid = false;
        break;
      }
      previous = values[index];
      have_previous = true;
      ++length;
    }
    if (valid && length > best) {
      best = length;
    }
  }
  return best;
}

}  // namespace

TEST_CASE(lis_handles_empty_singleton_sorted_reverse_and_duplicates) {
  const std::vector<std::int64_t> empty;
  REQUIRE(algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(empty)
              .indices.empty());
  REQUIRE(algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(empty)
              .indices.empty());

  const std::vector<std::int64_t> singleton{42};
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(singleton)
                 .indices,
             (std::vector<std::size_t>{0}));
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(singleton)
                 .indices,
             (std::vector<std::size_t>{0}));

  const std::vector<std::int64_t> sorted{1, 2, 3, 4, 5};
  const auto sorted_fast =
      algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(sorted);
  REQUIRE_EQ(sorted_fast.indices,
             (std::vector<std::size_t>{0, 1, 2, 3, 4}));

  const std::vector<std::int64_t> reverse{5, 4, 3, 2, 1};
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(reverse)
                 .indices.size(),
             std::size_t{1});
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(reverse)
                 .indices.size(),
             std::size_t{1});

  const std::vector<std::int64_t> duplicates{2, 2, 2, 2};
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(duplicates)
                 .indices,
             (std::vector<std::size_t>{0}));
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(duplicates)
                 .indices,
             (std::vector<std::size_t>{0}));
}

TEST_CASE(lis_reconstructs_known_strict_subsequence_and_handles_extremes) {
  const std::vector<std::int64_t> values{10, 9, 2, 5, 3, 7, 101, 18};
  const auto quadratic =
      algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(values);
  const auto fast =
      algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(values);
  REQUIRE_EQ(quadratic.indices.size(), std::size_t{4});
  REQUIRE_EQ(fast.indices.size(), std::size_t{4});
  require_valid_witness(values, quadratic);
  require_valid_witness(values, fast);

  const std::vector<std::int64_t> ties{3, 1, 2, 2, 3};
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(ties)
                 .indices,
             (std::vector<std::size_t>{1, 2, 4}));
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(ties)
                 .indices,
             (std::vector<std::size_t>{1, 2, 4}));

  const std::vector<std::int64_t> extremes{
      std::numeric_limits<std::int64_t>::min(), 0,
      std::numeric_limits<std::int64_t>::max()};
  REQUIRE_EQ(algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(extremes)
                 .indices.size(),
             std::size_t{3});
}

TEST_CASE(lis_algorithms_match_exhaustive_randomized_oracle) {
  std::mt19937_64 rng(0x1155AABBULL);
  std::uniform_int_distribution<int> count_distribution(0, 14);
  std::uniform_int_distribution<int> value_distribution(-10, 10);

  for (std::size_t trial = 0; trial < 420; ++trial) {
    const std::size_t count =
        static_cast<std::size_t>(count_distribution(rng));
    std::vector<std::int64_t> values;
    values.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      values.push_back(static_cast<std::int64_t>(value_distribution(rng)));
    }

    const auto quadratic =
        algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(values);
    const auto fast =
        algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(values);
    const std::size_t oracle = exhaustive_lis_length(values);

    REQUIRE_EQ(quadratic.indices.size(), oracle);
    REQUIRE_EQ(fast.indices.size(), oracle);
    require_valid_witness(values, quadratic);
    require_valid_witness(values, fast);
  }
}

TEST_CASE(lis_quadratic_and_nlogn_match_on_larger_random_inputs) {
  std::mt19937_64 rng(0xF45A11ULL);
  std::uniform_int_distribution<int> count_distribution(0, 220);
  std::uniform_int_distribution<int> value_distribution(-1000, 1000);

  for (std::size_t trial = 0; trial < 240; ++trial) {
    const std::size_t count =
        static_cast<std::size_t>(count_distribution(rng));
    std::vector<std::int64_t> values;
    values.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      values.push_back(static_cast<std::int64_t>(value_distribution(rng)));
    }

    const auto quadratic =
        algorithms::dynamic_programming::longest_increasing_subsequence_quadratic(values);
    const auto fast =
        algorithms::dynamic_programming::longest_increasing_subsequence_nlogn(values);
    REQUIRE_EQ(quadratic.indices.size(), fast.indices.size());
    require_valid_witness(values, quadratic);
    require_valid_witness(values, fast);
  }
}
