#include "algorithms/geometry/planar_diameter.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::geometry::PlanarDiameterResult;
using algorithms::geometry::Point2i;
using algorithms::geometry::planar_diameter;

namespace {

bool lex_less(Point2i left, Point2i right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

std::int64_t squared_distance(Point2i left, Point2i right) {
  const std::int64_t dx = static_cast<std::int64_t>(right.x) - left.x;
  const std::int64_t dy = static_cast<std::int64_t>(right.y) - left.y;
  return dx * dx + dy * dy;
}

PlanarDiameterResult make_result(Point2i first, Point2i second) {
  if (lex_less(second, first)) {
    std::swap(first, second);
  }
  return {first, second, squared_distance(first, second)};
}

bool result_better(const PlanarDiameterResult& candidate,
                   const PlanarDiameterResult& incumbent) {
  if (candidate.squared_distance != incumbent.squared_distance) {
    return candidate.squared_distance > incumbent.squared_distance;
  }
  if (!(candidate.first == incumbent.first)) {
    return lex_less(candidate.first, incumbent.first);
  }
  return lex_less(candidate.second, incumbent.second);
}

PlanarDiameterResult brute_force_diameter(const std::vector<Point2i>& points) {
  auto best = make_result(points[0], points[1]);
  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1U; j < points.size(); ++j) {
      const auto candidate = make_result(points[i], points[j]);
      if (result_better(candidate, best)) {
        best = candidate;
      }
    }
  }
  return best;
}

}  // namespace

TEST_CASE(planar_diameter_degenerate_duplicate_and_boundary_cases) {
  REQUIRE(!planar_diameter(std::vector<Point2i>{}).has_value());
  REQUIRE(!planar_diameter(std::vector<Point2i>{{5, -7}}).has_value());

  REQUIRE_EQ(planar_diameter(std::vector<Point2i>{{3, 4}, {3, 4}, {3, 4}})
                 .value(),
             PlanarDiameterResult({{3, 4}, {3, 4}, 0}));

  const std::vector<Point2i> extremes{{-1'000'000'000, -1'000'000'000},
                                      {1'000'000'000, 1'000'000'000}};
  REQUIRE_EQ(planar_diameter(extremes)->squared_distance,
             std::int64_t{8'000'000'000'000'000'000LL});
  REQUIRE_THROWS_AS(
      planar_diameter(std::vector<Point2i>{{0, 0}, {1'000'000'001, 0}}),
      std::out_of_range);
}

TEST_CASE(planar_diameter_hull_reduction_collinear_and_tie_semantics) {
  const std::vector<Point2i> square{{0, 0}, {2, 0}, {2, 2}, {0, 2},
                                    {1, 1}, {1, 0}, {0, 0}};
  const PlanarDiameterResult square_expected{{0, 0}, {2, 2}, 8};
  REQUIRE_EQ(planar_diameter(square).value(), square_expected);

  auto reversed = square;
  std::reverse(reversed.begin(), reversed.end());
  REQUIRE_EQ(planar_diameter(reversed).value(), square_expected);

  const std::vector<Point2i> line{{4, 0}, {-3, 0}, {-1, 0}, {2, 0}, {2, 0}};
  REQUIRE_EQ(planar_diameter(line).value(),
             PlanarDiameterResult({{-3, 0}, {4, 0}, 49}));

  const std::vector<Point2i> hull_with_interior{{-8, 0}, {0, -6}, {8, 0},
                                                 {0, 6},  {0, 0},  {1, 1},
                                                 {-1, 1}, {2, -1}};
  REQUIRE_EQ(planar_diameter(hull_with_interior).value(),
             brute_force_diameter(hull_with_interior));
}

TEST_CASE(planar_diameter_exhaustive_three_by_three_grid_subsets) {
  const std::vector<Point2i> grid{{-1, -1}, {-1, 0}, {-1, 1},
                                  {0, -1},  {0, 0},  {0, 1},
                                  {1, -1},  {1, 0},  {1, 1}};
  for (std::uint32_t mask = 0; mask < (std::uint32_t{1} << 9U); ++mask) {
    std::vector<Point2i> subset;
    for (std::size_t bit = 0; bit < grid.size(); ++bit) {
      if ((mask & (std::uint32_t{1} << bit)) != 0U) {
        subset.push_back(grid[bit]);
      }
    }
    if (subset.size() >= 2U) {
      REQUIRE_EQ(planar_diameter(subset).value(), brute_force_diameter(subset));
    }
  }
}

TEST_CASE(planar_diameter_randomized_quadratic_differential_and_replay) {
  std::mt19937_64 rng(0xD1A6E7E2ULL);
  for (std::size_t trial = 0; trial < 1500; ++trial) {
    const std::size_t count = 2U + static_cast<std::size_t>(rng() % 79U);
    std::vector<Point2i> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      points.push_back(Point2i{
          static_cast<std::int32_t>(
              static_cast<std::int64_t>(rng() % 2001U) - 1000),
          static_cast<std::int32_t>(
              static_cast<std::int64_t>(rng() % 2001U) - 1000)});
    }

    const auto expected = brute_force_diameter(points);
    REQUIRE_EQ(planar_diameter(points).value(), expected);

    auto shuffled = points;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    REQUIRE_EQ(planar_diameter(shuffled).value(), expected);
  }
}
