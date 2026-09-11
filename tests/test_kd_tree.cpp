#include "algorithms/geometry/kd_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::geometry::KdNearestResult;
using algorithms::geometry::KdTree2D;
using algorithms::geometry::Point2i;

namespace {

bool lex_less(Point2i left, Point2i right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

std::int64_t distance_squared(Point2i first, Point2i second) {
  const std::int64_t dx = static_cast<std::int64_t>(first.x) - second.x;
  const std::int64_t dy = static_cast<std::int64_t>(first.y) - second.y;
  return dx * dx + dy * dy;
}

std::optional<std::pair<Point2i, std::int64_t>> brute_force(
    std::vector<Point2i> points, Point2i query) {
  std::sort(points.begin(), points.end(), lex_less);
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.empty()) {
    return std::nullopt;
  }
  Point2i best = points.front();
  std::int64_t best_distance = distance_squared(best, query);
  for (const Point2i point : points) {
    const std::int64_t candidate = distance_squared(point, query);
    if (candidate < best_distance ||
        (candidate == best_distance && lex_less(point, best))) {
      best = point;
      best_distance = candidate;
    }
  }
  return std::pair<Point2i, std::int64_t>{best, best_distance};
}

}  // namespace

TEST_CASE(kd_tree_empty_validation_and_duplicate_semantics) {
  const KdTree2D empty(std::vector<Point2i>{});
  REQUIRE(empty.empty());
  REQUIRE_EQ(empty.size(), std::size_t{0});
  REQUIRE_EQ(empty.height(), std::size_t{0});
  REQUIRE(empty.valid_structure());
  REQUIRE(!empty.nearest({0, 0}).has_value());

  const std::vector<Point2i> repeated{{4, 5}, {4, 5}, {-2, 7}, {4, 5}};
  const KdTree2D tree(repeated);
  REQUIRE_EQ(tree.size(), std::size_t{2});
  REQUIRE(tree.valid_structure());
  REQUIRE_EQ(tree.nearest({4, 5})->point, Point2i({4, 5}));
  REQUIRE_EQ(tree.nearest({4, 5})->squared_distance, std::int64_t{0});

  REQUIRE_THROWS_AS(KdTree2D(std::vector<Point2i>{{1'000'000'001, 0}}),
                    std::out_of_range);
  REQUIRE_THROWS_AS(tree.nearest({0, -1'000'000'001}), std::out_of_range);
}

TEST_CASE(kd_tree_exact_ties_boundaries_and_pruning) {
  const std::vector<Point2i> square{{0, 0}, {2, 0}, {0, 2}, {2, 2}};
  const KdTree2D square_tree(square);
  const auto tie = square_tree.nearest({1, 1}).value();
  REQUIRE_EQ(tie.point, Point2i({0, 0}));
  REQUIRE_EQ(tie.squared_distance, std::int64_t{2});
  REQUIRE(square_tree.valid_structure());

  const KdTree2D extremes(std::vector<Point2i>{
      {-1'000'000'000, -1'000'000'000}, {1'000'000'000, 1'000'000'000}});
  REQUIRE_EQ(extremes.nearest({1'000'000'000, 1'000'000'000})->squared_distance,
             std::int64_t{0});
  REQUIRE_EQ(
      extremes.nearest({1'000'000'000, -1'000'000'000})->squared_distance,
      std::int64_t{4'000'000'000'000'000'000LL});

  std::vector<Point2i> separated;
  for (std::int32_t i = 0; i < 128; ++i) {
    separated.push_back({i, i});
    separated.push_back({1'000'000 - i, 1'000'000 - i});
  }
  const KdTree2D pruned(separated);
  const auto result = pruned.nearest({3, 4}).value();
  REQUIRE(result.visited_nodes < pruned.size());
  REQUIRE(result.pruned_subtrees > 0U);
  REQUIRE(pruned.valid_structure());
}

TEST_CASE(kd_tree_input_order_independence_and_balanced_height) {
  std::vector<Point2i> points;
  for (std::int32_t x = -8; x <= 8; ++x) {
    for (std::int32_t y = -3; y <= 3; ++y) {
      points.push_back({x * 11, y * 13});
    }
  }
  const KdTree2D first(points);
  std::mt19937_64 rng(0x4B4454524545ULL);
  std::shuffle(points.begin(), points.end(), rng);
  const KdTree2D second(points);
  REQUIRE(first.valid_structure());
  REQUIRE(second.valid_structure());
  REQUIRE_EQ(first.size(), second.size());
  REQUIRE_EQ(first.height(), second.height());
  REQUIRE(first.height() <=
          static_cast<std::size_t>(std::bit_width(first.size())));

  for (std::int32_t x = -100; x <= 100; x += 7) {
    const Point2i query{x, static_cast<std::int32_t>(x / 2)};
    const auto left = first.nearest(query).value();
    const auto right = second.nearest(query).value();
    REQUIRE_EQ(left.point, right.point);
    REQUIRE_EQ(left.squared_distance, right.squared_distance);
  }
}

TEST_CASE(kd_tree_randomized_against_full_scan) {
  std::mt19937_64 rng(0x4B44545245454F52ULL);
  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const std::size_t count = static_cast<std::size_t>(rng() % 81U);
    std::vector<Point2i> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      points.push_back(Point2i{
          static_cast<std::int32_t>(static_cast<std::int64_t>(rng() % 2001U) -
                                    1000),
          static_cast<std::int32_t>(static_cast<std::int64_t>(rng() % 2001U) -
                                    1000)});
    }

    const KdTree2D tree(points);
    REQUIRE(tree.valid_structure());
    std::vector<Point2i> unique = points;
    std::sort(unique.begin(), unique.end(), lex_less);
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    REQUIRE_EQ(tree.size(), unique.size());
    if (!tree.empty()) {
      REQUIRE(tree.height() <=
              static_cast<std::size_t>(std::bit_width(tree.size())));
    }

    for (std::size_t query_index = 0; query_index < 80U; ++query_index) {
      const Point2i query{
          static_cast<std::int32_t>(static_cast<std::int64_t>(rng() % 2401U) -
                                    1200),
          static_cast<std::int32_t>(static_cast<std::int64_t>(rng() % 2401U) -
                                    1200)};
      const auto expected = brute_force(points, query);
      const auto actual = tree.nearest(query);
      REQUIRE_EQ(actual.has_value(), expected.has_value());
      if (actual.has_value()) {
        REQUIRE_EQ(actual->point, expected->first);
        REQUIRE_EQ(actual->squared_distance, expected->second);
        REQUIRE(actual->visited_nodes >= 1U);
        REQUIRE(actual->visited_nodes <= tree.size());
        const KdNearestResult replay = tree.nearest(query).value();
        REQUIRE_EQ(replay, actual.value());
      }
    }
  }
}
