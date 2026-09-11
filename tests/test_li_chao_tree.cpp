#include "algorithms/data_structures/li_chao_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::data_structures::LiChaoMinTree;
using algorithms::data_structures::LiChaoQueryResult;

struct OracleLine {
  std::int64_t slope;
  std::int64_t intercept;
  std::size_t id;
};

std::int64_t oracle_evaluate(const OracleLine& line, std::int64_t x) {
  return line.slope * x + line.intercept;
}

std::optional<LiChaoQueryResult> oracle_query(
    const std::vector<OracleLine>& lines, std::int64_t x) {
  std::optional<LiChaoQueryResult> best;
  for (const OracleLine& line : lines) {
    const LiChaoQueryResult candidate{oracle_evaluate(line, x), line.id};
    if (!best.has_value() || candidate.value < best->value ||
        (candidate.value == best->value && candidate.line_id < best->line_id)) {
      best = candidate;
    }
  }
  return best;
}

TEST_CASE(li_chao_validates_domain_coefficients_and_empty_queries) {
  REQUIRE_THROWS_AS(LiChaoMinTree(3, 2), std::invalid_argument);
  REQUIRE_THROWS_AS(LiChaoMinTree(-1'000'000'001LL, 0), std::out_of_range);
  REQUIRE_THROWS_AS(LiChaoMinTree(0, 1'000'000'001LL), std::out_of_range);

  LiChaoMinTree tree(-10, 10);
  REQUIRE_EQ(tree.minimum_x(), -10);
  REQUIRE_EQ(tree.maximum_x(), 10);
  REQUIRE_EQ(tree.line_count(), 0U);
  REQUIRE_EQ(tree.allocated_node_count(), 0U);
  REQUIRE(!tree.query(0).has_value());
  REQUIRE_THROWS_AS(tree.query(-11), std::out_of_range);
  REQUIRE_THROWS_AS(tree.query(11), std::out_of_range);
  REQUIRE_THROWS_AS(tree.add_line(1'000'000'001LL, 0), std::out_of_range);
  REQUIRE_THROWS_AS(tree.add_line(0, -1'000'000'001LL), std::out_of_range);
  REQUIRE_EQ(tree.line_count(), 0U);
}

TEST_CASE(li_chao_crossings_parallel_lines_and_ties_are_deterministic) {
  LiChaoMinTree tree(-10, 10);
  const std::size_t rising = tree.add_line(1, 0);
  const std::size_t falling = tree.add_line(-1, 0);
  const std::size_t shifted = tree.add_line(1, -3);
  const std::size_t duplicate = tree.add_line(1, -3);
  REQUIRE_EQ(rising, 0U);
  REQUIRE_EQ(falling, 1U);
  REQUIRE_EQ(shifted, 2U);
  REQUIRE_EQ(duplicate, 3U);
  REQUIRE_EQ(tree.line_count(), 4U);

  REQUIRE_EQ(tree.query(-10), std::optional<LiChaoQueryResult>({-13, shifted}));
  REQUIRE_EQ(tree.query(0), std::optional<LiChaoQueryResult>({-3, shifted}));
  REQUIRE_EQ(tree.query(10), std::optional<LiChaoQueryResult>({-10, falling}));

  LiChaoMinTree tie_tree(-4, 4);
  const std::size_t first = tie_tree.add_line(1, 0);
  const std::size_t second = tie_tree.add_line(-1, 0);
  REQUIRE_EQ(tie_tree.query(0), std::optional<LiChaoQueryResult>({0, first}));
  REQUIRE_EQ(tie_tree.query(-4),
             std::optional<LiChaoQueryResult>({-4, first}));
  REQUIRE_EQ(tie_tree.query(4),
             std::optional<LiChaoQueryResult>({-4, second}));
}

TEST_CASE(li_chao_exact_arithmetic_domain_covers_full_public_boundaries) {
  constexpr std::int64_t limit = LiChaoMinTree::kAbsoluteInputLimit;
  LiChaoMinTree tree(-limit, limit);
  const std::size_t first = tree.add_line(limit, limit);
  const std::size_t second = tree.add_line(-limit, -limit);

  REQUIRE_EQ(tree.query(-limit),
             std::optional<LiChaoQueryResult>({-999'999'999'000'000'000LL,
                                               first}));
  REQUIRE_EQ(tree.query(limit),
             std::optional<LiChaoQueryResult>({-1'000'000'001'000'000'000LL,
                                               second}));

  LiChaoMinTree singleton(7, 7);
  const std::size_t singleton_line = singleton.add_line(-8, 5);
  REQUIRE_EQ(singleton.query(7),
             std::optional<LiChaoQueryResult>({-51, singleton_line}));
}

TEST_CASE(li_chao_randomized_differential_matches_full_line_scan) {
  std::mt19937_64 rng(0x11C4A0D5ULL);
  std::uniform_int_distribution<std::int64_t> coefficient(-100, 100);
  std::uniform_int_distribution<std::int64_t> coordinate(-50, 50);
  std::uniform_int_distribution<int> action(0, 99);

  for (std::size_t trial = 0; trial < 500U; ++trial) {
    LiChaoMinTree tree(-50, 50);
    std::vector<OracleLine> oracle_lines;

    for (std::size_t step = 0; step < 160U; ++step) {
      if (oracle_lines.empty() ||
          (oracle_lines.size() < 80U && action(rng) < 55)) {
        const std::int64_t slope = coefficient(rng);
        const std::int64_t intercept = coefficient(rng);
        const std::size_t id = tree.add_line(slope, intercept);
        REQUIRE_EQ(id, oracle_lines.size());
        oracle_lines.push_back(OracleLine{slope, intercept, id});
      } else {
        const std::int64_t x = coordinate(rng);
        REQUIRE_EQ(tree.query(x), oracle_query(oracle_lines, x));
      }
      REQUIRE_EQ(tree.line_count(), oracle_lines.size());
    }

    for (std::int64_t x = -50; x <= 50; ++x) {
      REQUIRE_EQ(tree.query(x), oracle_query(oracle_lines, x));
    }

    REQUIRE(tree.allocated_node_count() <= tree.line_count());
  }
}

}  // namespace
