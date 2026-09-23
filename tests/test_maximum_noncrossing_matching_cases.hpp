#pragma once

#include "algorithms/dynamic_programming/maximum_noncrossing_matching.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::dynamic_programming::maximum_noncrossing_matching;

namespace maximum_noncrossing_matching_test_detail {

using Edge = std::pair<std::size_t, std::size_t>;

inline Edge normalized(Edge edge) {
  if (edge.first > edge.second) {
    std::swap(edge.first, edge.second);
  }
  return edge;
}

inline bool crosses(const Edge first_raw, const Edge second_raw) {
  const Edge first = normalized(first_raw);
  const Edge second = normalized(second_raw);

  return (first.first < second.first &&
          second.first < first.second &&
          first.second < second.second) ||
         (second.first < first.first &&
          first.first < second.second &&
          second.second < first.second);
}

inline void require_valid_witness(
    const std::size_t vertex_count,
    const std::vector<Edge>& allowed_edges,
    const std::vector<Edge>& witness) {
  std::set<Edge> allowed;
  for (Edge edge : allowed_edges) {
    allowed.insert(normalized(edge));
  }

  std::vector<bool> used(vertex_count, false);
  for (std::size_t index = 0U; index < witness.size(); ++index) {
    const Edge edge = normalized(witness[index]);
    REQUIRE(edge.first < edge.second);
    REQUIRE(edge.second < vertex_count);
    REQUIRE(allowed.contains(edge));
    REQUIRE(!used[edge.first]);
    REQUIRE(!used[edge.second]);
    used[edge.first] = true;
    used[edge.second] = true;

    for (std::size_t prior = 0U; prior < index; ++prior) {
      REQUIRE(!crosses(edge, witness[prior]));
    }
  }
}

// Independent exact oracle. It enumerates every partial matching on the ordered
// vertex set and only at completed leaves checks whether all chosen edges are
// allowed and pairwise noncrossing. It does not use interval decomposition.
inline std::size_t brute_force_optimum(
    const std::size_t vertex_count,
    const std::vector<Edge>& allowed_edges) {
  std::set<Edge> allowed;
  for (Edge edge : allowed_edges) {
    allowed.insert(normalized(edge));
  }

  std::vector<bool> used(vertex_count, false);
  std::vector<Edge> chosen;
  std::size_t best = 0U;

  const auto enumerate = [&](auto&& self) -> void {
    std::size_t first = 0U;
    while (first < vertex_count && used[first]) {
      ++first;
    }

    if (first == vertex_count) {
      for (const Edge edge : chosen) {
        if (!allowed.contains(normalized(edge))) {
          return;
        }
      }
      for (std::size_t i = 0U; i < chosen.size(); ++i) {
        for (std::size_t j = i + 1U; j < chosen.size(); ++j) {
          if (crosses(chosen[i], chosen[j])) {
            return;
          }
        }
      }
      best = std::max(best, chosen.size());
      return;
    }

    // Leave the first free vertex unmatched.
    used[first] = true;
    self(self);
    used[first] = false;

    for (std::size_t second = first + 1U;
         second < vertex_count; ++second) {
      if (used[second]) {
        continue;
      }
      used[first] = true;
      used[second] = true;
      chosen.emplace_back(first, second);
      self(self);
      chosen.pop_back();
      used[second] = false;
      used[first] = false;
    }
  };

  enumerate(enumerate);
  return best;
}

inline std::vector<Edge> graph_from_mask(
    const std::size_t vertex_count,
    std::uint64_t mask) {
  std::vector<Edge> edges;
  std::size_t bit = 0U;
  for (std::size_t first = 0U; first < vertex_count; ++first) {
    for (std::size_t second = first + 1U;
         second < vertex_count; ++second, ++bit) {
      if (((mask >> bit) & UINT64_C(1)) != 0U) {
        edges.emplace_back(first, second);
      }
    }
  }
  return edges;
}

}  // namespace maximum_noncrossing_matching_test_detail

TEST_CASE(maximum_noncrossing_matching_empty_and_validation) {
  using namespace maximum_noncrossing_matching_test_detail;

  REQUIRE(maximum_noncrossing_matching(
              0U, std::vector<Edge>{})
              .empty());
  REQUIRE(maximum_noncrossing_matching(
              1U, std::vector<Edge>{})
              .empty());

  REQUIRE_THROWS_AS(
      maximum_noncrossing_matching(
          3U, std::vector<Edge>{{0U, 3U}}),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      maximum_noncrossing_matching(
          3U, std::vector<Edge>{{1U, 1U}}),
      std::invalid_argument);
}

TEST_CASE(maximum_noncrossing_matching_nested_disjoint_and_crossing) {
  using namespace maximum_noncrossing_matching_test_detail;

  const std::vector<Edge> complete_four{
      {0U, 1U}, {0U, 2U}, {0U, 3U},
      {1U, 2U}, {1U, 3U}, {2U, 3U}};
  const auto complete_witness =
      maximum_noncrossing_matching(4U, complete_four);
  REQUIRE_EQ(complete_witness.size(), 2U);
  require_valid_witness(4U, complete_four, complete_witness);

  const std::vector<Edge> nested{
      {0U, 3U}, {1U, 2U}};
  const auto nested_witness =
      maximum_noncrossing_matching(4U, nested);
  REQUIRE_EQ(nested_witness.size(), 2U);
  require_valid_witness(4U, nested, nested_witness);

  const std::vector<Edge> crossing_only{
      {0U, 2U}, {1U, 3U}};
  const auto crossing_witness =
      maximum_noncrossing_matching(4U, crossing_only);
  REQUIRE_EQ(crossing_witness.size(), 1U);
  require_valid_witness(4U, crossing_only, crossing_witness);
}

TEST_CASE(maximum_noncrossing_matching_duplicates_and_direction) {
  using namespace maximum_noncrossing_matching_test_detail;

  const std::vector<Edge> edges{
      {3U, 0U}, {0U, 3U}, {2U, 1U}, {1U, 2U},
      {0U, 1U}, {2U, 3U}};
  const auto witness = maximum_noncrossing_matching(4U, edges);

  REQUIRE_EQ(witness.size(), 2U);
  require_valid_witness(4U, edges, witness);
  REQUIRE(
      witness ==
      maximum_noncrossing_matching(4U, edges));
}

TEST_CASE(maximum_noncrossing_matching_exhaustive_small_graphs) {
  using namespace maximum_noncrossing_matching_test_detail;

  for (std::size_t vertex_count = 0U;
       vertex_count <= 5U; ++vertex_count) {
    const std::size_t edge_count =
        vertex_count * (vertex_count - (vertex_count == 0U ? 0U : 1U)) /
        2U;
    REQUIRE(edge_count < 63U);
    const std::uint64_t graph_count =
        UINT64_C(1) << edge_count;

    for (std::uint64_t mask = 0U;
         mask < graph_count; ++mask) {
      const std::vector<Edge> edges =
          graph_from_mask(vertex_count, mask);
      const auto witness =
          maximum_noncrossing_matching(vertex_count, edges);

      require_valid_witness(vertex_count, edges, witness);
      REQUIRE_EQ(
          witness.size(),
          brute_force_optimum(vertex_count, edges));
    }
  }
}

TEST_CASE(maximum_noncrossing_matching_random_larger_graphs) {
  using namespace maximum_noncrossing_matching_test_detail;

  std::mt19937_64 random(UINT64_C(0x4E4F4E43524F5353));

  for (std::size_t trial = 0U; trial < 360U; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(random() % 9U);

    std::vector<Edge> edges;
    for (std::size_t first = 0U;
         first < vertex_count; ++first) {
      for (std::size_t second = first + 1U;
           second < vertex_count; ++second) {
        if ((random() % 3U) != 0U) {
          if ((random() & 1ULL) == 0ULL) {
            edges.emplace_back(first, second);
          } else {
            edges.emplace_back(second, first);
          }
          if ((random() % 7U) == 0U) {
            edges.push_back(edges.back());
          }
        }
      }
    }

    const auto witness =
        maximum_noncrossing_matching(vertex_count, edges);
    require_valid_witness(vertex_count, edges, witness);
    REQUIRE_EQ(
        witness.size(),
        brute_force_optimum(vertex_count, edges));
  }
}
