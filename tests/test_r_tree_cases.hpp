#pragma once

#include "algorithms/data_structures/r_tree.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::data_structures::RTree2D;

namespace r_tree_test_detail {

inline bool overlaps(
    const RTree2D::Rect a,
    const RTree2D::Rect b) {
  return a.min_x <= b.max_x && b.min_x <= a.max_x &&
         a.min_y <= b.max_y && b.min_y <= a.max_y;
}

inline std::vector<RTree2D::Id> brute_query(
    const std::vector<RTree2D::Rect>& rectangles,
    const RTree2D::Rect query) {
  std::vector<RTree2D::Id> result;
  for (std::size_t id = 0U; id < rectangles.size(); ++id) {
    if (overlaps(rectangles[id], query)) {
      result.push_back(id);
    }
  }
  return result;
}

inline void require_query_matches(
    const RTree2D& tree,
    const std::vector<RTree2D::Rect>& rectangles,
    const RTree2D::Rect query) {
  REQUIRE(tree.query_intersect(query) ==
          brute_query(rectangles, query));
}

}  // namespace r_tree_test_detail

TEST_CASE(r_tree_empty_invalid_and_closed_boundary_contract) {
  using namespace r_tree_test_detail;

  RTree2D tree;
  REQUIRE(tree.empty());
  REQUIRE_EQ(tree.size(), 0U);
  REQUIRE_EQ(tree.height(), 0U);
  REQUIRE(tree.valid_structure());

  const RTree2D::Rect valid{0, 0, 10, 10};
  REQUIRE(tree.query_intersect(valid).empty());

  REQUIRE_THROWS_AS(
      tree.insert(RTree2D::Rect{5, 0, 4, 1}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      tree.query_intersect(RTree2D::Rect{0, 3, 1, 2}),
      std::invalid_argument);

  std::vector<RTree2D::Rect> rectangles;
  rectangles.push_back(valid);
  REQUIRE_EQ(tree.insert(valid), 0U);
  rectangles.push_back(RTree2D::Rect{10, 3, 12, 7});
  REQUIRE_EQ(tree.insert(rectangles.back()), 1U);
  rectangles.push_back(RTree2D::Rect{11, 11, 20, 20});
  REQUIRE_EQ(tree.insert(rectangles.back()), 2U);

  REQUIRE_EQ(tree.height(), 1U);
  REQUIRE(tree.valid_structure());

  require_query_matches(
      tree, rectangles, RTree2D::Rect{10, 5, 10, 5});
  require_query_matches(
      tree, rectangles, RTree2D::Rect{11, 0, 11, 2});
}

TEST_CASE(r_tree_handles_extreme_and_degenerate_rectangles) {
  using namespace r_tree_test_detail;

  const std::int32_t lo =
      std::numeric_limits<std::int32_t>::min();
  const std::int32_t hi =
      std::numeric_limits<std::int32_t>::max();

  RTree2D tree;
  std::vector<RTree2D::Rect> rectangles{
      {lo, lo, hi, hi},
      {lo, 0, lo, 0},
      {hi, hi, hi, hi},
      {-1, -1, 1, 1},
      {0, lo, 0, hi},
      {lo, 0, hi, 0},
      {100, 100, 100, 100},
      {-100, -100, -100, -100},
      {7, 7, 8, 8}};

  for (std::size_t id = 0U; id < rectangles.size(); ++id) {
    REQUIRE_EQ(tree.insert(rectangles[id]), id);
  }

  REQUIRE(tree.height() >= 2U);
  REQUIRE(tree.valid_structure());

  require_query_matches(
      tree, rectangles, RTree2D::Rect{0, 0, 0, 0});
  require_query_matches(
      tree, rectangles, RTree2D::Rect{hi, hi, hi, hi});
  require_query_matches(
      tree, rectangles, RTree2D::Rect{lo, lo, lo, lo});
}

TEST_CASE(r_tree_repeated_identical_rectangles_survive_splits) {
  using namespace r_tree_test_detail;

  RTree2D tree;
  std::vector<RTree2D::Rect> rectangles;
  const RTree2D::Rect same{-5, -5, 5, 5};

  for (std::size_t index = 0U; index < 96U; ++index) {
    rectangles.push_back(same);
    REQUIRE_EQ(tree.insert(same), index);
    if (index % 11U == 0U) {
      REQUIRE(tree.valid_structure());
    }
  }

  REQUIRE(tree.height() >= 3U);
  REQUIRE(tree.valid_structure());

  const auto all = tree.query_intersect(RTree2D::Rect{0, 0, 0, 0});
  REQUIRE_EQ(all.size(), rectangles.size());
  for (std::size_t id = 0U; id < all.size(); ++id) {
    REQUIRE_EQ(all[id], id);
  }

  REQUIRE(tree.query_intersect(
              RTree2D::Rect{6, 6, 7, 7})
              .empty());
}

TEST_CASE(r_tree_grid_forces_multilevel_balanced_structure) {
  using namespace r_tree_test_detail;

  RTree2D tree;
  std::vector<RTree2D::Rect> rectangles;

  for (std::int32_t y = 0; y < 20; ++y) {
    for (std::int32_t x = 0; x < 20; ++x) {
      const RTree2D::Rect rect{
          x * 10, y * 10,
          x * 10 + 3, y * 10 + 4};
      const std::size_t expected_id = rectangles.size();
      rectangles.push_back(rect);
      REQUIRE_EQ(tree.insert(rect), expected_id);
    }
  }

  REQUIRE_EQ(tree.size(), 400U);
  REQUIRE(tree.height() >= 3U);
  REQUIRE(tree.valid_structure());

  require_query_matches(
      tree, rectangles, RTree2D::Rect{50, 50, 133, 147});
  require_query_matches(
      tree, rectangles, RTree2D::Rect{-100, -100, -1, -1});
  require_query_matches(
      tree, rectangles, RTree2D::Rect{0, 0, 199, 199});
}

TEST_CASE(r_tree_randomized_overlap_queries_match_direct_scan) {
  using namespace r_tree_test_detail;

  std::mt19937_64 random(0x52545245455EEDULL);
  RTree2D tree;
  std::vector<RTree2D::Rect> rectangles;

  const auto random_rect = [&]() {
    const std::int32_t x1 =
        static_cast<std::int32_t>(random() % 2001U) - 1000;
    const std::int32_t x2 =
        static_cast<std::int32_t>(random() % 2001U) - 1000;
    const std::int32_t y1 =
        static_cast<std::int32_t>(random() % 2001U) - 1000;
    const std::int32_t y2 =
        static_cast<std::int32_t>(random() % 2001U) - 1000;
    return RTree2D::Rect{
        std::min(x1, x2), std::min(y1, y2),
        std::max(x1, x2), std::max(y1, y2)};
  };

  for (std::size_t step = 0U; step < 1200U; ++step) {
    const RTree2D::Rect rect = random_rect();
    REQUIRE_EQ(tree.insert(rect), rectangles.size());
    rectangles.push_back(rect);

    if (step % 53U == 0U) {
      REQUIRE(tree.valid_structure());
      for (std::size_t query = 0U; query < 17U; ++query) {
        require_query_matches(tree, rectangles, random_rect());
      }
    }
  }

  REQUIRE_EQ(tree.size(), rectangles.size());
  REQUIRE(tree.height() >= 3U);
  REQUIRE(tree.valid_structure());

  for (std::size_t query = 0U; query < 500U; ++query) {
    require_query_matches(tree, rectangles, random_rect());
  }
}
