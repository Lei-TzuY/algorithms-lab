#pragma once

#include "algorithms/graphs/nagamochi_ibaraki_certificate.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::graphs::Vertex;
using algorithms::graphs::nagamochi_ibaraki_sparse_certificate;

namespace nagamochi_ibaraki_test_detail {

using EdgePair = std::pair<Vertex, Vertex>;

inline std::size_t cut_size(
    const std::vector<EdgePair>& edges,
    const std::uint64_t mask) {
  std::size_t count = 0U;
  for (const auto& [first, second] : edges) {
    const bool first_side =
        ((mask >> first) & UINT64_C(1)) != 0U;
    const bool second_side =
        ((mask >> second) & UINT64_C(1)) != 0U;
    if (first_side != second_side) {
      ++count;
    }
  }
  return count;
}

inline std::vector<EdgePair> selected_edges(
    const std::vector<EdgePair>& edges,
    const std::vector<std::size_t>& ids) {
  std::vector<EdgePair> result;
  result.reserve(ids.size());
  for (const std::size_t id : ids) {
    REQUIRE(id < edges.size());
    result.push_back(edges[id]);
  }
  return result;
}

inline void require_certificate(
    const std::size_t vertex_count,
    const std::vector<EdgePair>& edges,
    const std::size_t threshold) {
  REQUIRE(vertex_count < 63U);

  const auto ids = nagamochi_ibaraki_sparse_certificate(
      vertex_count, edges, threshold);
  REQUIRE(ids == nagamochi_ibaraki_sparse_certificate(
                     vertex_count, edges, threshold));

  std::vector<std::size_t> sorted_ids = ids;
  std::sort(sorted_ids.begin(), sorted_ids.end());
  REQUIRE(std::adjacent_find(sorted_ids.begin(), sorted_ids.end()) ==
          sorted_ids.end());

  for (const std::size_t id : ids) {
    REQUIRE(id < edges.size());
    REQUIRE(edges[id].first != edges[id].second);
  }

  if (vertex_count <= 1U) {
    REQUIRE(ids.empty());
  } else if (threshold != 0U) {
    const std::size_t simple_bound =
        threshold <=
                std::numeric_limits<std::size_t>::max() /
                    (vertex_count - 1U)
            ? threshold * (vertex_count - 1U)
            : std::numeric_limits<std::size_t>::max();
    REQUIRE(ids.size() <= simple_bound);
  }

  const std::vector<EdgePair> certificate =
      selected_edges(edges, ids);

  if (vertex_count == 0U) {
    REQUIRE(ids.empty());
    return;
  }

  const std::uint64_t full =
      (UINT64_C(1) << vertex_count) - UINT64_C(1);
  for (std::uint64_t mask = UINT64_C(1);
       mask < full; ++mask) {
    const std::size_t original = cut_size(edges, mask);
    const std::size_t sparse = cut_size(certificate, mask);

    REQUIRE(sparse <= original);
    REQUIRE(sparse >= std::min(threshold, original));
    if (original < threshold) {
      REQUIRE_EQ(sparse, original);
    }
  }
}

}  // namespace nagamochi_ibaraki_test_detail

TEST_CASE(nagamochi_ibaraki_zero_threshold_and_validation) {
  using namespace nagamochi_ibaraki_test_detail;

  REQUIRE(
      nagamochi_ibaraki_sparse_certificate(
          0U, std::vector<EdgePair>{}, 7U)
          .empty());

  const std::vector<EdgePair> one_vertex_loops{
      {0U, 0U}, {0U, 0U}};
  REQUIRE(
      nagamochi_ibaraki_sparse_certificate(
          1U, one_vertex_loops, 7U)
          .empty());

  const std::vector<EdgePair> edges{
      {0U, 1U}, {1U, 2U}, {2U, 0U}};

  REQUIRE(
      nagamochi_ibaraki_sparse_certificate(3U, edges, 0U).empty());

  REQUIRE_THROWS_AS(
      nagamochi_ibaraki_sparse_certificate(
          3U, std::vector<EdgePair>{{0U, 3U}}, 1U),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      nagamochi_ibaraki_sparse_certificate(
          0U, std::vector<EdgePair>{{0U, 0U}}, 0U),
      std::out_of_range);
}

TEST_CASE(nagamochi_ibaraki_triangle_thresholds) {
  using namespace nagamochi_ibaraki_test_detail;

  const std::vector<EdgePair> triangle{
      {0U, 1U}, {1U, 2U}, {2U, 0U}};

  require_certificate(3U, triangle, 0U);
  require_certificate(3U, triangle, 1U);
  require_certificate(3U, triangle, 2U);
  require_certificate(3U, triangle, 3U);

  REQUIRE_EQ(
      nagamochi_ibaraki_sparse_certificate(
          3U, triangle, 1U)
          .size(),
      2U);
  REQUIRE_EQ(
      nagamochi_ibaraki_sparse_certificate(
          3U, triangle, 2U)
          .size(),
      3U);
}

TEST_CASE(nagamochi_ibaraki_parallel_edges_and_loops) {
  using namespace nagamochi_ibaraki_test_detail;

  const std::vector<EdgePair> edges{
      {0U, 0U},
      {0U, 1U},
      {0U, 1U},
      {0U, 1U},
      {0U, 1U},
      {0U, 1U},
      {1U, 1U},
  };

  for (std::size_t threshold = 0U;
       threshold <= 7U; ++threshold) {
    require_certificate(2U, edges, threshold);
  }

  REQUIRE_EQ(
      nagamochi_ibaraki_sparse_certificate(
          2U, edges, 3U),
      (std::vector<std::size_t>{1U, 2U, 3U}));
  REQUIRE_EQ(
      nagamochi_ibaraki_sparse_certificate(
          2U, edges, 99U),
      (std::vector<std::size_t>{1U, 2U, 3U, 4U, 5U}));
}

TEST_CASE(nagamochi_ibaraki_disconnected_graph) {
  using namespace nagamochi_ibaraki_test_detail;

  const std::vector<EdgePair> edges{
      {0U, 1U}, {1U, 2U}, {2U, 0U},
      {3U, 4U}, {3U, 4U},
      {5U, 5U},
  };

  for (std::size_t threshold = 0U;
       threshold <= 4U; ++threshold) {
    require_certificate(6U, edges, threshold);
  }
}

TEST_CASE(nagamochi_ibaraki_random_multigraph_all_cuts) {
  using namespace nagamochi_ibaraki_test_detail;

  std::mt19937_64 random(UINT64_C(0x4E4147414D4F4348));

  for (std::size_t trial = 0U; trial < 420U; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(random() % 8U);
    const std::size_t edge_count =
        static_cast<std::size_t>(random() % 24U);

    std::vector<EdgePair> edges;
    edges.reserve(edge_count);
    for (std::size_t edge = 0U; edge < edge_count; ++edge) {
      const Vertex first =
          static_cast<Vertex>(random() % vertex_count);
      const Vertex second =
          static_cast<Vertex>(random() % vertex_count);
      edges.emplace_back(first, second);
    }

    for (std::size_t threshold = 0U;
         threshold <= 4U; ++threshold) {
      require_certificate(vertex_count, edges, threshold);
    }
  }
}
