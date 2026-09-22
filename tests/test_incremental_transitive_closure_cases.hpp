#pragma once

#include "algorithms/graphs/incremental_transitive_closure.hpp"

#include <cstddef>
#include <cstdint>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

namespace incremental_transitive_closure_test_detail {

using algorithms::graphs::IncrementalTransitiveClosure;
using algorithms::graphs::Vertex;

using EdgeMatrix = std::vector<std::vector<unsigned char>>;

inline std::vector<unsigned char> bfs_reachability(
    const EdgeMatrix& edges, const Vertex source) {
  std::vector<unsigned char> seen(edges.size(), 0U);
  std::queue<Vertex> ready;
  seen[source] = 1U;
  ready.push(source);

  while (!ready.empty()) {
    const Vertex from = ready.front();
    ready.pop();
    for (Vertex to = 0U; to < edges.size(); ++to) {
      if (edges[from][to] != 0U && seen[to] == 0U) {
        seen[to] = 1U;
        ready.push(to);
      }
    }
  }
  return seen;
}

inline void require_matches_bfs(const IncrementalTransitiveClosure& closure,
                                const EdgeMatrix& edges) {
  REQUIRE(closure.valid_invariants());
  REQUIRE_EQ(closure.vertex_count(), edges.size());

  for (Vertex from = 0U; from < edges.size(); ++from) {
    const auto expected = bfs_reachability(edges, from);
    std::size_t count = 0U;
    for (Vertex to = 0U; to < edges.size(); ++to) {
      REQUIRE_EQ(closure.reachable(from, to),
                 expected[to] != static_cast<unsigned char>(0U));
      REQUIRE_EQ(closure.same_strong_component(from, to),
                 expected[to] != static_cast<unsigned char>(0U) &&
                     bfs_reachability(edges, to)[from] !=
                         static_cast<unsigned char>(0U));
      if (expected[to] != static_cast<unsigned char>(0U)) {
        ++count;
      }
    }
    REQUIRE_EQ(closure.reachable_count_from(from), count);
  }
}

}  // namespace incremental_transitive_closure_test_detail

TEST_CASE(incremental_transitive_closure_reflexive_and_explicit_edge_semantics) {
  using namespace incremental_transitive_closure_test_detail;

  IncrementalTransitiveClosure empty(0U);
  REQUIRE_EQ(empty.vertex_count(), 0U);
  REQUIRE_EQ(empty.edge_count(), 0U);
  REQUIRE(empty.valid_invariants());
  REQUIRE_THROWS_AS(empty.reachable(0U, 0U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.add_edge(0U, 0U), std::out_of_range);

  IncrementalTransitiveClosure closure(4U);
  for (Vertex vertex = 0U; vertex < 4U; ++vertex) {
    REQUIRE(closure.reachable(vertex, vertex));
    REQUIRE_EQ(closure.reachable_count_from(vertex), 1U);
  }

  REQUIRE(closure.add_edge(0U, 1U));
  REQUIRE(closure.add_edge(1U, 2U));
  REQUIRE(closure.reachable(0U, 2U));
  REQUIRE(!closure.explicit_edge(0U, 2U));

  // An implied relation can still be inserted as one new explicit graph edge.
  REQUIRE(closure.add_edge(0U, 2U));
  REQUIRE(closure.explicit_edge(0U, 2U));
  REQUIRE(!closure.add_edge(0U, 2U));
  REQUIRE_EQ(closure.edge_count(), 3U);
  REQUIRE(closure.valid_invariants());
}

TEST_CASE(incremental_transitive_closure_cycles_and_scc_relation) {
  using namespace incremental_transitive_closure_test_detail;

  IncrementalTransitiveClosure closure(5U);
  REQUIRE(closure.add_edge(0U, 1U));
  REQUIRE(closure.add_edge(1U, 2U));
  REQUIRE(closure.add_edge(2U, 3U));

  REQUIRE(!closure.same_strong_component(1U, 3U));
  REQUIRE(closure.would_create_cycle(3U, 1U));
  REQUIRE(!closure.would_create_cycle(0U, 4U));

  REQUIRE(closure.add_edge(3U, 1U));
  REQUIRE(closure.same_strong_component(1U, 2U));
  REQUIRE(closure.same_strong_component(1U, 3U));
  REQUIRE(!closure.same_strong_component(0U, 1U));
  REQUIRE(closure.reachable(0U, 3U));
  REQUIRE(!closure.reachable(3U, 0U));

  REQUIRE(closure.would_create_cycle(4U, 4U));
  REQUIRE(closure.add_edge(4U, 4U));
  REQUIRE_EQ(closure.edge_count(), 5U);
  REQUIRE(closure.valid_invariants());
}

TEST_CASE(incremental_transitive_closure_crosses_bitset_word_boundaries) {
  using namespace incremental_transitive_closure_test_detail;

  constexpr std::size_t vertex_count = 130U;
  IncrementalTransitiveClosure closure(vertex_count);

  for (Vertex vertex = 0U; vertex + 1U < vertex_count; ++vertex) {
    REQUIRE(closure.add_edge(vertex, vertex + 1U));
  }

  REQUIRE_EQ(closure.reachable_count_from(0U), vertex_count);
  REQUIRE_EQ(closure.reachable_count_from(63U), vertex_count - 63U);
  REQUIRE_EQ(closure.reachable_count_from(64U), vertex_count - 64U);
  REQUIRE_EQ(closure.reachable_count_from(129U), 1U);
  REQUIRE(closure.reachable(0U, 64U));
  REQUIRE(closure.reachable(0U, 129U));
  REQUIRE(!closure.reachable(129U, 0U));
  REQUIRE(closure.valid_invariants());
}

TEST_CASE(incremental_transitive_closure_randomized_matches_bfs_oracle) {
  using namespace incremental_transitive_closure_test_detail;

  std::mt19937_64 random(0xC105E5ULL);
  for (std::size_t trial = 0U; trial < 80U; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(random() % 18U);
    IncrementalTransitiveClosure closure(vertex_count);
    EdgeMatrix edges(
        vertex_count,
        std::vector<unsigned char>(vertex_count, 0U));
    std::size_t explicit_count = 0U;

    for (std::size_t step = 0U;
         step < 100U && vertex_count != 0U; ++step) {
      const Vertex from =
          static_cast<Vertex>(random() % vertex_count);
      const Vertex to =
          static_cast<Vertex>(random() % vertex_count);

      const bool expected_cycle =
          from == to || bfs_reachability(edges, to)[from] !=
                            static_cast<unsigned char>(0U);
      REQUIRE_EQ(closure.would_create_cycle(from, to), expected_cycle);

      const bool expected_inserted = edges[from][to] == 0U;
      REQUIRE_EQ(closure.add_edge(from, to), expected_inserted);
      if (expected_inserted) {
        edges[from][to] = 1U;
        ++explicit_count;
      }
      REQUIRE_EQ(closure.edge_count(), explicit_count);
      REQUIRE(closure.explicit_edge(from, to));
      REQUIRE(closure.reachable(from, to));

      if ((step % 5U) == 0U) {
        require_matches_bfs(closure, edges);
      }
    }

    if (vertex_count != 0U) {
      require_matches_bfs(closure, edges);
    } else {
      REQUIRE(closure.valid_invariants());
    }
  }
}
