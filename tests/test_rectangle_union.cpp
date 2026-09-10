#include "algorithms/geometry/rectangle_union.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::geometry::AxisAlignedRectangle;
using algorithms::geometry::rectangle_union_area;

std::int64_t brute_unit_cell_union_area(
    const std::vector<AxisAlignedRectangle>& rectangles, int minimum,
    int maximum) {
  std::int64_t area = 0;
  for (int x = minimum; x < maximum; ++x) {
    for (int y = minimum; y < maximum; ++y) {
      bool covered = false;
      for (const auto& rectangle : rectangles) {
        if (rectangle.min_x <= x && x < rectangle.max_x &&
            rectangle.min_y <= y && y < rectangle.max_y) {
          covered = true;
          break;
        }
      }
      if (covered) {
        ++area;
      }
    }
  }
  return area;
}

TEST_CASE(rectangle_union_area_basic_overlap_and_degenerate_cases) {
  REQUIRE_EQ(rectangle_union_area({}), 0);

  const std::vector<AxisAlignedRectangle> single = {{0, 0, 2, 3}};
  REQUIRE_EQ(rectangle_union_area(single), 6);

  const std::vector<AxisAlignedRectangle> degenerate = {
      {0, 0, 0, 4}, {-3, 2, 5, 2}};
  REQUIRE_EQ(rectangle_union_area(degenerate), 0);

  const std::vector<AxisAlignedRectangle> overlap = {
      {0, 0, 3, 2}, {1, 1, 4, 3}};
  REQUIRE_EQ(rectangle_union_area(overlap), 10);

  const std::vector<AxisAlignedRectangle> duplicate_and_contained = {
      {-2, -2, 2, 2}, {-2, -2, 2, 2}, {-1, -1, 1, 1}};
  REQUIRE_EQ(rectangle_union_area(duplicate_and_contained), 16);
}

TEST_CASE(rectangle_union_area_batches_touching_edges_without_double_counting) {
  const std::vector<AxisAlignedRectangle> rectangles = {
      {0, 0, 1, 2}, {1, 0, 2, 2}, {0, 2, 2, 3}, {2, 1, 3, 3}};
  REQUIRE_EQ(rectangle_union_area(rectangles), 8);

  auto reversed = rectangles;
  std::reverse(reversed.begin(), reversed.end());
  REQUIRE_EQ(rectangle_union_area(reversed), 8);
}

TEST_CASE(rectangle_union_area_exact_domain_boundary_and_validation) {
  constexpr std::int32_t limit = 1'000'000'000;
  const std::vector<AxisAlignedRectangle> full_domain = {
      {-limit, -limit, limit, limit}};
  REQUIRE_EQ(rectangle_union_area(full_domain), 4'000'000'000'000'000'000LL);

  const std::vector<AxisAlignedRectangle> bad_x = {{2, 0, 1, 1}};
  REQUIRE_THROWS_AS(rectangle_union_area(bad_x), std::invalid_argument);
  const std::vector<AxisAlignedRectangle> bad_y = {{0, 2, 1, 1}};
  REQUIRE_THROWS_AS(rectangle_union_area(bad_y), std::invalid_argument);

  const std::vector<AxisAlignedRectangle> too_large = {
      {0, 0, 1'000'000'001, 1}};
  REQUIRE_THROWS_AS(rectangle_union_area(too_large), std::out_of_range);
  const std::vector<AxisAlignedRectangle> too_small = {
      {-1'000'000'001, 0, 0, 1}};
  REQUIRE_THROWS_AS(rectangle_union_area(too_small), std::out_of_range);
}

TEST_CASE(rectangle_union_area_exhaustive_small_rectangle_pairs) {
  std::vector<AxisAlignedRectangle> catalog;
  for (int min_x = 0; min_x < 2; ++min_x) {
    for (int max_x = min_x + 1; max_x <= 2; ++max_x) {
      for (int min_y = 0; min_y < 2; ++min_y) {
        for (int max_y = min_y + 1; max_y <= 2; ++max_y) {
          catalog.push_back(AxisAlignedRectangle{min_x, min_y, max_x, max_y});
        }
      }
    }
  }

  for (std::size_t first = 0; first < catalog.size(); ++first) {
    for (std::size_t second = first; second < catalog.size(); ++second) {
      const std::vector<AxisAlignedRectangle> input = {catalog[first],
                                                       catalog[second]};
      REQUIRE_EQ(rectangle_union_area(input),
                 brute_unit_cell_union_area(input, 0, 2));
    }
  }
}

TEST_CASE(rectangle_union_area_randomized_differential_and_permutation) {
  std::mt19937_64 rng(0x5E33'B0A7ULL);
  std::uniform_int_distribution<int> count_distribution(0, 12);
  std::uniform_int_distribution<int> coordinate_distribution(-5, 5);

  for (int trial = 0; trial < 2'000; ++trial) {
    const int count = count_distribution(rng);
    std::vector<AxisAlignedRectangle> rectangles;
    rectangles.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
      int x1 = coordinate_distribution(rng);
      int x2 = coordinate_distribution(rng);
      int y1 = coordinate_distribution(rng);
      int y2 = coordinate_distribution(rng);
      if (x2 < x1) {
        std::swap(x1, x2);
      }
      if (y2 < y1) {
        std::swap(y1, y2);
      }
      rectangles.push_back(AxisAlignedRectangle{x1, y1, x2, y2});
    }

    const auto expected = brute_unit_cell_union_area(rectangles, -5, 5);
    REQUIRE_EQ(rectangle_union_area(rectangles), expected);

    std::shuffle(rectangles.begin(), rectangles.end(), rng);
    REQUIRE_EQ(rectangle_union_area(rectangles), expected);
  }
}

}  // namespace
