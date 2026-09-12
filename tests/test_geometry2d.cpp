#include "algorithms/geometry/dominance3d.hpp"
#include "algorithms/geometry/geometry2d.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

using algorithms::geometry::Orientation;
using algorithms::geometry::Point2i;
using algorithms::geometry::Point3i;
using algorithms::geometry::convex_hull;
using algorithms::geometry::dominance_counts_3d;
using algorithms::geometry::on_segment;
using algorithms::geometry::orientation;
using algorithms::geometry::segments_intersect;

namespace {

std::int64_t raw_cross(Point2i a, Point2i b, Point2i c) {
  const std::int64_t abx = static_cast<std::int64_t>(b.x) - a.x;
  const std::int64_t aby = static_cast<std::int64_t>(b.y) - a.y;
  const std::int64_t acx = static_cast<std::int64_t>(c.x) - a.x;
  const std::int64_t acy = static_cast<std::int64_t>(c.y) - a.y;
  return abx * acy - aby * acx;
}

std::int64_t squared_distance(Point2i a, Point2i b) {
  const std::int64_t dx = static_cast<std::int64_t>(b.x) - a.x;
  const std::int64_t dy = static_cast<std::int64_t>(b.y) - a.y;
  return dx * dx + dy * dy;
}

bool lex_less(Point2i a, Point2i b) {
  return a.x < b.x || (a.x == b.x && a.y < b.y);
}

std::vector<Point2i> jarvis_hull(std::vector<Point2i> points) {
  std::sort(points.begin(), points.end(), lex_less);
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.size() <= 2U) {
    return points;
  }

  bool all_collinear = true;
  for (std::size_t i = 2; i < points.size(); ++i) {
    if (raw_cross(points[0], points[1], points[i]) != 0) {
      all_collinear = false;
      break;
    }
  }
  if (all_collinear) {
    return {points.front(), points.back()};
  }

  std::vector<Point2i> hull;
  const Point2i start = points.front();
  Point2i current = start;
  do {
    hull.push_back(current);
    Point2i next = points[0] == current ? points[1] : points[0];
    for (const Point2i candidate : points) {
      if (candidate == current) {
        continue;
      }
      const std::int64_t turn = raw_cross(current, next, candidate);
      if (turn < 0 ||
          (turn == 0 && squared_distance(current, candidate) >
                            squared_distance(current, next))) {
        next = candidate;
      }
    }
    current = next;
  } while (!(current == start));
  return hull;
}

bool closest_result_less(const algorithms::geometry::ClosestPairResult& left,
                         const algorithms::geometry::ClosestPairResult& right) {
  if (left.squared_distance != right.squared_distance) {
    return left.squared_distance < right.squared_distance;
  }
  if (!(left.first == right.first)) {
    return lex_less(left.first, right.first);
  }
  return lex_less(left.second, right.second);
}

algorithms::geometry::ClosestPairResult brute_force_closest_pair(
    const std::vector<Point2i>& points) {
  auto make_result = [](Point2i first, Point2i second) {
    if (lex_less(second, first)) {
      std::swap(first, second);
    }
    return algorithms::geometry::ClosestPairResult{
        first, second, squared_distance(first, second)};
  };

  auto best = make_result(points[0], points[1]);
  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1U; j < points.size(); ++j) {
      const auto candidate = make_result(points[i], points[j]);
      if (closest_result_less(candidate, best)) {
        best = candidate;
      }
    }
  }
  return best;
}

std::vector<std::uint64_t> quadratic_dominance3d(
    std::span<const Point3i> points) {
  std::vector<std::uint64_t> result(points.size(), 0U);
  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = 0; j < points.size(); ++j) {
      if (points[j].x <= points[i].x && points[j].y <= points[i].y &&
          points[j].z <= points[i].z) {
        ++result[i];
      }
    }
  }
  return result;
}

}  // namespace

TEST_CASE(geometry_exact_predicates_and_intersections) {
  REQUIRE_EQ(orientation({0, 0}, {2, 0}, {1, 1}),
             Orientation::Counterclockwise);
  REQUIRE_EQ(orientation({0, 0}, {1, 1}, {2, 0}), Orientation::Clockwise);
  REQUIRE_EQ(orientation({0, 0}, {1, 1}, {2, 2}), Orientation::Collinear);
  REQUIRE(on_segment({0, 0}, {4, 4}, {2, 2}));
  REQUIRE(!on_segment({0, 0}, {4, 4}, {5, 5}));
  REQUIRE(!on_segment({0, 0}, {4, 4}, {2, 3}));
  REQUIRE(segments_intersect({0, 0}, {4, 4}, {0, 4}, {4, 0}));
  REQUIRE(segments_intersect({0, 0}, {2, 0}, {2, 0}, {3, 1}));
  REQUIRE(segments_intersect({0, 0}, {4, 0}, {2, 0}, {6, 0}));
  REQUIRE(!segments_intersect({0, 0}, {1, 0}, {2, 0}, {3, 0}));
  REQUIRE(segments_intersect({1, 1}, {1, 1}, {1, 1}, {1, 1}));
  REQUIRE(!segments_intersect({1, 1}, {1, 1}, {0, 0}, {2, 0}));
  REQUIRE_EQ(orientation({-1'000'000'000, -1'000'000'000},
                         {1'000'000'000, -1'000'000'000},
                         {-1'000'000'000, 1'000'000'000}),
             Orientation::Counterclockwise);
  REQUIRE_THROWS_AS(orientation({1'000'000'001, 0}, {0, 0}, {0, 1}),
                    std::out_of_range);
}

TEST_CASE(geometry_convex_hull_degenerate_and_deterministic) {
  REQUIRE(convex_hull(std::vector<Point2i>{}).empty());
  REQUIRE_EQ(convex_hull(std::vector<Point2i>{{2, 3}}),
             std::vector<Point2i>({{2, 3}}));
  REQUIRE_EQ(convex_hull(std::vector<Point2i>{{0, 0}, {1, 0}, {0, 0}}),
             std::vector<Point2i>({{0, 0}, {1, 0}}));

  const std::vector<Point2i> square{{0, 0}, {2, 0}, {2, 2}, {0, 2},
                                    {1, 1}, {1, 0}, {0, 0}};
  REQUIRE_EQ(convex_hull(square),
             std::vector<Point2i>({{0, 0}, {2, 0}, {2, 2}, {0, 2}}));

  const std::vector<Point2i> line{{3, 3}, {1, 1}, {2, 2}, {1, 1}, {-1, -1}};
  REQUIRE_EQ(convex_hull(line), std::vector<Point2i>({{-1, -1}, {3, 3}}));
}

TEST_CASE(geometry_randomized_hull_against_jarvis_and_segment_symmetry) {
  std::mt19937_64 rng(0xC0A7EULL);
  for (std::size_t trial = 0; trial < 500; ++trial) {
    const std::size_t count = static_cast<std::size_t>(rng() % 26U);
    std::vector<Point2i> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const auto x = static_cast<std::int32_t>(
          static_cast<std::int64_t>(rng() % 101U) - 50);
      const auto y = static_cast<std::int32_t>(
          static_cast<std::int64_t>(rng() % 101U) - 50);
      points.push_back({x, y});
    }

    const auto actual = convex_hull(points);
    const auto expected = jarvis_hull(points);
    REQUIRE_EQ(actual, expected);

    if (actual.size() >= 3U) {
      for (std::size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(raw_cross(actual[i], actual[(i + 1U) % actual.size()],
                          actual[(i + 2U) % actual.size()]) > 0);
      }
      for (const Point2i point : points) {
        for (std::size_t i = 0; i < actual.size(); ++i) {
          REQUIRE(raw_cross(actual[i], actual[(i + 1U) % actual.size()],
                            point) >= 0);
        }
      }
    }
  }

  std::mt19937_64 segment_rng(0x5E6A3EULL);
  for (std::size_t trial = 0; trial < 5000; ++trial) {
    auto point = [&]() {
      return Point2i{
          static_cast<std::int32_t>(
              static_cast<std::int64_t>(segment_rng() % 101U) - 50),
          static_cast<std::int32_t>(
              static_cast<std::int64_t>(segment_rng() % 101U) - 50)};
    };
    const Point2i a = point();
    const Point2i b = point();
    const Point2i c = point();
    const Point2i d = point();
    const bool value = segments_intersect(a, b, c, d);
    REQUIRE_EQ(value, segments_intersect(b, a, c, d));
    REQUIRE_EQ(value, segments_intersect(a, b, d, c));
    REQUIRE_EQ(value, segments_intersect(c, d, a, b));
  }
}

TEST_CASE(geometry_closest_pair_degenerate_duplicate_and_boundary_cases) {
  using algorithms::geometry::ClosestPairResult;
  using algorithms::geometry::closest_pair;

  REQUIRE(!closest_pair(std::vector<Point2i>{}).has_value());
  REQUIRE(!closest_pair(std::vector<Point2i>{{5, -7}}).has_value());
  REQUIRE_EQ(closest_pair(std::vector<Point2i>{{3, 4}, {0, 0}}).value(),
             ClosestPairResult({{0, 0}, {3, 4}, 25}));

  const std::vector<Point2i> duplicates{{4, 5}, {-2, 7}, {4, 5}, {-2, 7}};
  REQUIRE_EQ(closest_pair(duplicates).value(),
             ClosestPairResult({{-2, 7}, {-2, 7}, 0}));

  const std::vector<Point2i> extremes{{-1'000'000'000, -1'000'000'000},
                                      {1'000'000'000, 1'000'000'000}};
  REQUIRE_EQ(closest_pair(extremes)->squared_distance,
             std::int64_t{8'000'000'000'000'000'000LL});
  REQUIRE_THROWS_AS(
      closest_pair(std::vector<Point2i>{{0, 0}, {1'000'000'001, 0}}),
      std::out_of_range);
}

TEST_CASE(geometry_closest_pair_ties_cross_split_and_grid) {
  using algorithms::geometry::ClosestPairResult;
  using algorithms::geometry::closest_pair;

  const std::vector<Point2i> square{{1, 1}, {0, 1}, {1, 0}, {0, 0}};
  const ClosestPairResult expected{{0, 0}, {0, 1}, 1};
  REQUIRE_EQ(closest_pair(square).value(), expected);
  auto reversed = square;
  std::reverse(reversed.begin(), reversed.end());
  REQUIRE_EQ(closest_pair(reversed).value(), expected);

  const std::vector<Point2i> cross_split{{-100, 1000}, {-50, -1000}, {-1, 7},
                                          {1, 7},      {50, 1000},   {100, -1000}};
  REQUIRE_EQ(closest_pair(cross_split).value(),
             ClosestPairResult({{-1, 7}, {1, 7}, 4}));

  std::vector<Point2i> grid;
  for (std::int32_t x = -5; x <= 5; ++x) {
    for (std::int32_t y = -5; y <= 5; ++y) {
      grid.push_back({x * 10, y * 10});
    }
  }
  REQUIRE_EQ(closest_pair(grid).value(), brute_force_closest_pair(grid));
}

TEST_CASE(geometry_closest_pair_randomized_quadratic_differential) {
  using algorithms::geometry::closest_pair;

  std::mt19937_64 rng(0xC105E57ULL);
  for (std::size_t trial = 0; trial < 1500; ++trial) {
    const std::size_t count = 2U + static_cast<std::size_t>(rng() % 63U);
    std::vector<Point2i> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      points.push_back(Point2i{
          static_cast<std::int32_t>(
              static_cast<std::int64_t>(rng() % 2001U) - 1000),
          static_cast<std::int32_t>(
              static_cast<std::int64_t>(rng() % 2001U) - 1000)});
    }
    const auto expected = brute_force_closest_pair(points);
    REQUIRE_EQ(closest_pair(points).value(), expected);

    auto shuffled = points;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    REQUIRE_EQ(closest_pair(shuffled).value(), expected);
  }
}

TEST_CASE(dominance3d_empty_singleton_duplicates_and_incomparables) {
  REQUIRE(dominance_counts_3d({}).empty());
  const std::vector<Point3i> singleton{{7, -3, 11}};
  REQUIRE_EQ(dominance_counts_3d(singleton), std::vector<std::uint64_t>{1});

  const std::vector<Point3i> points{{0, 0, 0}, {0, 0, 0}, {0, 0, 0},
                                    {1, 1, 1}, {-1, 2, 0}, {2, -1, 0}};
  REQUIRE_EQ(dominance_counts_3d(points), quadratic_dominance3d(points));
}

TEST_CASE(dominance3d_equal_x_and_extreme_coordinates) {
  const auto lo = std::numeric_limits<std::int64_t>::min();
  const auto hi = std::numeric_limits<std::int64_t>::max();
  const std::vector<Point3i> points{{0, -2, 5}, {0, -1, 4}, {0, -1, 5},
                                    {0, 0, 3},  {lo, lo, lo}, {hi, hi, hi},
                                    {lo, hi, lo}, {hi, lo, hi}};
  REQUIRE_EQ(dominance_counts_3d(points), quadratic_dominance3d(points));
}

TEST_CASE(dominance3d_permutation_invariance_and_monotone_chain) {
  std::vector<Point3i> points;
  for (std::int64_t value = -64; value <= 64; ++value) {
    points.push_back(Point3i{value, value, value});
  }
  REQUIRE_EQ(dominance_counts_3d(points), quadratic_dominance3d(points));

  std::mt19937_64 rng(0xCD03D0ULL);
  std::shuffle(points.begin(), points.end(), rng);
  REQUIRE_EQ(dominance_counts_3d(points), quadratic_dominance3d(points));
}

TEST_CASE(dominance3d_randomized_differential) {
  std::mt19937_64 rng(0xD031A4CEULL);
  std::uniform_int_distribution<int> size_dist(0, 90);
  std::uniform_int_distribution<std::int64_t> coord_dist(-6, 6);
  for (int trial = 0; trial < 900; ++trial) {
    const int raw_size = size_dist(rng);
    const std::size_t size = static_cast<std::size_t>(raw_size);
    std::vector<Point3i> points;
    points.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
      points.push_back(Point3i{coord_dist(rng), coord_dist(rng), coord_dist(rng)});
    }
    REQUIRE_EQ(dominance_counts_3d(points), quadratic_dominance3d(points));
  }
}
