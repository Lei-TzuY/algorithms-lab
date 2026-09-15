#pragma once

#include "algorithms/data_structures/point_quadtree_multiset.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::data_structures::QuadtreePointMultiplicity;
using algorithms::data_structures::PointQuadtreeMultiset;
using algorithms::geometry::Point2i;

struct PointQuadtreePointLess {
  bool operator()(Point2i a, Point2i b) const noexcept {
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
  }
};

std::size_t point_quadtree_oracle_count_closed(const std::map<Point2i, std::size_t, PointQuadtreePointLess>& oracle,
                                Point2i low, Point2i high) {
  std::size_t count = 0;
  for (const auto& [point, multiplicity] : oracle) {
    if (point.x >= low.x && point.x <= high.x &&
        point.y >= low.y && point.y <= high.y) {
      count += multiplicity;
    }
  }
  return count;
}

std::vector<QuadtreePointMultiplicity> point_quadtree_oracle_report_closed(
    const std::map<Point2i, std::size_t, PointQuadtreePointLess>& oracle, Point2i low,
    Point2i high) {
  std::vector<QuadtreePointMultiplicity> result;
  for (const auto& [point, multiplicity] : oracle) {
    if (point.x >= low.x && point.x <= high.x &&
        point.y >= low.y && point.y <= high.y) {
      result.push_back({point, multiplicity});
    }
  }
  return result;
}

TEST_CASE(point_quadtree_boundaries_duplicates_and_ranges) {
  PointQuadtreeMultiset tree;
  REQUIRE(tree.empty());
  REQUIRE(tree.valid_structure());
  REQUIRE_EQ(tree.size(), std::size_t{0});
  REQUIRE_EQ(tree.distinct_size(), std::size_t{0});
  REQUIRE_EQ(tree.node_count(), std::size_t{0});
  REQUIRE_EQ(tree.height(), std::size_t{0});

  constexpr std::int32_t lo = PointQuadtreeMultiset::kMinCoordinate;
  constexpr std::int32_t hi = PointQuadtreeMultiset::kMaxCoordinate;
  tree.insert({lo, lo});
  tree.insert({hi, hi});
  tree.insert({0, 0});
  tree.insert({0, 0});
  REQUIRE_EQ(tree.size(), std::size_t{4});
  REQUIRE_EQ(tree.distinct_size(), std::size_t{3});
  REQUIRE_EQ(tree.count({0, 0}), std::size_t{2});
  REQUIRE_EQ(tree.count_closed({lo, lo}, {0, 0}), std::size_t{3});
  const std::vector<QuadtreePointMultiplicity> expected{{{lo, lo}, 1}, {{0, 0}, 2}};
  REQUIRE_EQ(tree.report_closed({lo, lo}, {0, 0}), expected);
  REQUIRE(tree.valid_structure());

  REQUIRE(tree.erase_one({0, 0}));
  REQUIRE_EQ(tree.count({0, 0}), std::size_t{1});
  REQUIRE(tree.erase_one({0, 0}));
  REQUIRE_EQ(tree.count({0, 0}), std::size_t{0});
  REQUIRE(!tree.erase_one({0, 0}));
  REQUIRE(tree.valid_structure());

  REQUIRE_THROWS_AS(tree.insert({hi + 1, 0}), std::out_of_range);
  REQUIRE_THROWS_AS(tree.count({0, hi + 1}), std::out_of_range);
  REQUIRE_THROWS_AS(tree.count_closed({1, 0}, {0, 1}), std::out_of_range);
  REQUIRE_THROWS_AS(tree.report_closed({lo - 1, 0}, {0, 0}), std::out_of_range);
}

TEST_CASE(point_quadtree_split_and_compaction_are_observable) {
  PointQuadtreeMultiset tree;
  const std::vector<Point2i> points{
      {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0},
      {6, 0}, {7, 0}, {8, 0}, {9, 0},
  };
  for (Point2i point : points) tree.insert(point);
  REQUIRE(tree.valid_structure());
  REQUIRE(tree.node_count() > 1);
  REQUIRE(tree.height() > 20);
  REQUIRE_EQ(tree.distinct_size(), points.size());

  REQUIRE(tree.erase_one(points.back()));
  REQUIRE(tree.valid_structure());
  REQUIRE_EQ(tree.distinct_size(), PointQuadtreeMultiset::kLeafCapacity);
  REQUIRE_EQ(tree.node_count(), std::size_t{1});
  REQUIRE_EQ(tree.height(), std::size_t{1});
  for (std::size_t i = 0; i + 1 < points.size(); ++i) {
    REQUIRE_EQ(tree.count(points[i]), std::size_t{1});
  }
}

TEST_CASE(point_quadtree_closed_report_is_sorted_and_multiplicity_exact) {
  PointQuadtreeMultiset tree;
  const std::vector<Point2i> input{{5, 5}, {-2, 4}, {5, 5}, {0, 0},
                                   {9, -3}, {-2, 4}, {1, 7}, {-8, -8}};
  for (Point2i point : input) tree.insert(point);
  const std::vector<QuadtreePointMultiplicity> expected{{{-2, 4}, 2}, {{0, 0}, 1},
                                                 {{1, 7}, 1}, {{5, 5}, 2}};
  REQUIRE_EQ(tree.report_closed({-3, 0}, {5, 7}), expected);
  REQUIRE_EQ(tree.count_closed({-3, 0}, {5, 7}), std::size_t{6});
  REQUIRE(tree.valid_structure());
}

TEST_CASE(point_quadtree_randomized_differential_trace) {
  PointQuadtreeMultiset tree;
  std::map<Point2i, std::size_t, PointQuadtreePointLess> oracle;
  std::mt19937_64 rng(0x5155414454524545ULL);
  std::size_t oracle_size = 0;

  for (std::size_t step = 0; step < 20000; ++step) {
    const Point2i point{static_cast<std::int32_t>(rng() % 81U) - 40,
                        static_cast<std::int32_t>(rng() % 81U) - 40};
    const std::uint64_t op = rng() % 10U;
    if (op < 5U) {
      tree.insert(point);
      ++oracle[point];
      ++oracle_size;
    } else if (op < 8U) {
      const bool actual = tree.erase_one(point);
      const auto it = oracle.find(point);
      const bool expected = it != oracle.end();
      REQUIRE_EQ(actual, expected);
      if (expected) {
        --it->second;
        --oracle_size;
        if (it->second == 0) oracle.erase(it);
      }
    } else {
      Point2i a{static_cast<std::int32_t>(rng() % 101U) - 50,
                static_cast<std::int32_t>(rng() % 101U) - 50};
      Point2i b{static_cast<std::int32_t>(rng() % 101U) - 50,
                static_cast<std::int32_t>(rng() % 101U) - 50};
      const Point2i low{std::min(a.x, b.x), std::min(a.y, b.y)};
      const Point2i high{std::max(a.x, b.x), std::max(a.y, b.y)};
      REQUIRE_EQ(tree.count_closed(low, high), point_quadtree_oracle_count_closed(oracle, low, high));
      REQUIRE_EQ(tree.report_closed(low, high), point_quadtree_oracle_report_closed(oracle, low, high));
    }

    REQUIRE_EQ(tree.size(), oracle_size);
    REQUIRE_EQ(tree.distinct_size(), oracle.size());
    REQUIRE_EQ(tree.count(point), oracle.contains(point) ? oracle.at(point) : 0U);
    if ((step % 29U) == 0U) REQUIRE(tree.valid_structure());
  }
  REQUIRE(tree.valid_structure());
  REQUIRE_EQ(tree.count_closed({-50, -50}, {50, 50}), oracle_size);
  REQUIRE_EQ(tree.report_closed({-50, -50}, {50, 50}),
             point_quadtree_oracle_report_closed(oracle, {-50, -50}, {50, 50}));
}

}  // namespace
