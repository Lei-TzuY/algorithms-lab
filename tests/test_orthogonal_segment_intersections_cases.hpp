#pragma once

#include "algorithms/geometry/orthogonal_segment_intersections.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using algorithms::geometry::HorizontalSegment2i;
using algorithms::geometry::OrthogonalSegmentIntersection;
using algorithms::geometry::Point2i;
using algorithms::geometry::VerticalSegment2i;
using algorithms::geometry::orthogonal_segment_intersections;

std::vector<OrthogonalSegmentIntersection> naive_orthogonal_intersections(
    std::span<const HorizontalSegment2i> horizontals,
    std::span<const VerticalSegment2i> verticals) {
  std::vector<OrthogonalSegmentIntersection> result;
  for (std::size_t hi = 0; hi < horizontals.size(); ++hi) {
    const auto& h = horizontals[hi];
    const std::int32_t left = std::min(h.first.x, h.second.x);
    const std::int32_t right = std::max(h.first.x, h.second.x);
    for (std::size_t vi = 0; vi < verticals.size(); ++vi) {
      const auto& v = verticals[vi];
      const std::int32_t low = std::min(v.first.y, v.second.y);
      const std::int32_t high = std::max(v.first.y, v.second.y);
      if (v.first.x >= left && v.first.x <= right && h.first.y >= low &&
          h.first.y <= high) {
        result.push_back({Point2i{v.first.x, h.first.y}, hi, vi});
      }
    }
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return std::tie(left.point.x, left.point.y, left.horizontal_index,
                    left.vertical_index) <
           std::tie(right.point.x, right.point.y, right.horizontal_index,
                    right.vertical_index);
  });
  return result;
}

TEST_CASE(orthogonal_segment_intersections_validation) {
  const std::vector<HorizontalSegment2i> bad_horizontal{{{0, 0}, {0, 1}}};
  const std::vector<VerticalSegment2i> no_verticals;
  REQUIRE_THROWS_AS(orthogonal_segment_intersections(bad_horizontal, no_verticals),
                    std::invalid_argument);

  const std::vector<HorizontalSegment2i> degenerate_horizontal{{{1, 2}, {1, 2}}};
  REQUIRE_THROWS_AS(
      orthogonal_segment_intersections(degenerate_horizontal, no_verticals),
      std::invalid_argument);

  const std::vector<VerticalSegment2i> bad_vertical{{{0, 0}, {1, 0}}};
  const std::vector<HorizontalSegment2i> no_horizontals;
  REQUIRE_THROWS_AS(orthogonal_segment_intersections(no_horizontals, bad_vertical),
                    std::invalid_argument);

  const std::vector<HorizontalSegment2i> outside{{{1'000'000'001, 0}, {0, 0}}};
  REQUIRE_THROWS_AS(orthogonal_segment_intersections(outside, no_verticals),
                    std::out_of_range);
}

TEST_CASE(orthogonal_segment_intersections_inclusive_endpoints_and_multiplicity) {
  const std::vector<HorizontalSegment2i> horizontals{
      {{0, 0}, {4, 0}}, {{5, 2}, {2, 2}}, {{4, 0}, {6, 0}},
      {{-1'000'000'000, -3}, {1'000'000'000, -3}}};
  const std::vector<VerticalSegment2i> verticals{
      {{0, -1}, {0, 0}}, {{4, 2}, {4, -1}},
      {{1'000'000'000, -3}, {1'000'000'000, 5}}};

  const std::vector<OrthogonalSegmentIntersection> expected{
      {{0, 0}, 0, 0}, {{4, 0}, 0, 1}, {{4, 0}, 2, 1},
      {{4, 2}, 1, 1}, {{1'000'000'000, -3}, 3, 2}};
  REQUIRE_EQ(orthogonal_segment_intersections(horizontals, verticals), expected);
  REQUIRE_EQ(orthogonal_segment_intersections(horizontals, verticals), expected);
}

TEST_CASE(orthogonal_segment_intersections_empty_and_no_hits) {
  const std::vector<HorizontalSegment2i> empty_h;
  const std::vector<VerticalSegment2i> empty_v;
  REQUIRE(orthogonal_segment_intersections(empty_h, empty_v).empty());

  const std::vector<HorizontalSegment2i> horizontals{{{0, 0}, {1, 0}}};
  const std::vector<VerticalSegment2i> verticals{{{2, -1}, {2, 1}}};
  REQUIRE(orthogonal_segment_intersections(horizontals, verticals).empty());
}

TEST_CASE(orthogonal_segment_intersections_randomized_differential) {
  std::mt19937_64 rng(0x0A71'0A1ULL);
  for (std::size_t trial = 0; trial < 1200U; ++trial) {
    const std::size_t h_count = static_cast<std::size_t>(rng() % 19U);
    const std::size_t v_count = static_cast<std::size_t>(rng() % 19U);
    std::vector<HorizontalSegment2i> horizontals;
    std::vector<VerticalSegment2i> verticals;
    horizontals.reserve(h_count);
    verticals.reserve(v_count);

    const auto coordinate = [&]() {
      return static_cast<std::int32_t>(static_cast<std::int64_t>(rng() % 31U) - 15);
    };

    for (std::size_t index = 0; index < h_count; ++index) {
      const std::int32_t y = coordinate();
      std::int32_t first_x = coordinate();
      std::int32_t second_x = coordinate();
      if (first_x == second_x) {
        second_x = (second_x == 15) ? 14 : static_cast<std::int32_t>(second_x + 1);
      }
      if ((rng() & 1U) != 0U) {
        std::swap(first_x, second_x);
      }
      horizontals.push_back({Point2i{first_x, y}, Point2i{second_x, y}});
    }
    for (std::size_t index = 0; index < v_count; ++index) {
      const std::int32_t x = coordinate();
      std::int32_t first_y = coordinate();
      std::int32_t second_y = coordinate();
      if (first_y == second_y) {
        second_y = (second_y == 15) ? 14 : static_cast<std::int32_t>(second_y + 1);
      }
      if ((rng() & 1U) != 0U) {
        std::swap(first_y, second_y);
      }
      verticals.push_back({Point2i{x, first_y}, Point2i{x, second_y}});
    }

    REQUIRE_EQ(orthogonal_segment_intersections(horizontals, verticals),
               naive_orthogonal_intersections(horizontals, verticals));
  }
}
}  // namespace
