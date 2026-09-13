#pragma once

#include "algorithms/graphs/tutte_polynomial.hpp"
#include "algorithms/graphs/spanning_tree_count.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::Graph;
using algorithms::graphs::TutteTerm;
using LogicalEdge = std::pair<std::size_t, std::size_t>;

class OracleDsu {
 public:
  explicit OracleDsu(std::size_t size) : parent_(size), components_(size) {
    for (std::size_t index = 0; index < size; ++index) {
      parent_[index] = index;
    }
  }

  std::size_t find(std::size_t vertex) {
    while (parent_[vertex] != vertex) {
      vertex = parent_[vertex];
    }
    return vertex;
  }

  void unite(std::size_t first, std::size_t second) {
    first = find(first);
    second = find(second);
    if (first != second) {
      parent_[second] = first;
      --components_;
    }
  }

  [[nodiscard]] std::size_t components() const noexcept { return components_; }

 private:
  std::vector<std::size_t> parent_;
  std::size_t components_;
};

std::size_t subset_rank(std::size_t vertex_count,
                        const std::vector<LogicalEdge>& edges,
                        std::uint64_t mask) {
  OracleDsu dsu(vertex_count);
  for (std::size_t index = 0; index < edges.size(); ++index) {
    if (((mask >> index) & 1U) == 0U) {
      continue;
    }
    const auto [first, second] = edges[index];
    if (first != second) {
      dsu.unite(first, second);
    }
  }
  return vertex_count - dsu.components();
}

bool oracle_connected(std::size_t vertex_count, const std::vector<LogicalEdge>& edges) {
  if (vertex_count == 0) {
    return false;
  }
  OracleDsu dsu(vertex_count);
  for (const auto& [first, second] : edges) {
    if (first != second) {
      dsu.unite(first, second);
    }
  }
  return dsu.components() == 1;
}

std::uint64_t evaluate_at_one_one(const std::vector<TutteTerm>& terms) {
  std::uint64_t value = 0;
  for (const auto& term : terms) {
    value += term.coefficient;
  }
  return value;
}

std::int64_t binomial(std::size_t n, std::size_t k) {
  if (k > n) {
    return 0;
  }
  k = std::min(k, n - k);
  std::int64_t result = 1;
  for (std::size_t index = 1; index <= k; ++index) {
    result = (result * static_cast<std::int64_t>(n - k + index)) /
             static_cast<std::int64_t>(index);
  }
  return result;
}

std::vector<TutteTerm> subset_rank_oracle(std::size_t vertex_count,
                                          const std::vector<LogicalEdge>& edges) {
  const auto edge_count = edges.size();
  const auto full_mask = edge_count == 64 ? ~std::uint64_t{0}
                                           : ((std::uint64_t{1} << edge_count) - 1U);
  const auto full_rank = subset_rank(vertex_count, edges, full_mask);

  std::map<std::pair<std::size_t, std::size_t>, std::int64_t> coefficients;
  const auto subset_count = std::uint64_t{1} << edge_count;
  for (std::uint64_t mask = 0; mask < subset_count; ++mask) {
    const auto rank = subset_rank(vertex_count, edges, mask);
    const auto selected = static_cast<std::size_t>(std::popcount(mask));
    const auto x_power = full_rank - rank;
    const auto y_power = selected - rank;

    for (std::size_t x_degree = 0; x_degree <= x_power; ++x_degree) {
      for (std::size_t y_degree = 0; y_degree <= y_power; ++y_degree) {
        const auto magnitude = binomial(x_power, x_degree) *
                               binomial(y_power, y_degree);
        const auto negative = ((x_power - x_degree + y_power - y_degree) & 1U) != 0U;
        coefficients[{x_degree, y_degree}] += negative ? -magnitude : magnitude;
      }
    }
  }

  std::vector<TutteTerm> result;
  for (const auto& [degree, coefficient] : coefficients) {
    REQUIRE(coefficient >= 0);
    if (coefficient != 0) {
      result.push_back(TutteTerm{degree.first, degree.second,
                                 static_cast<std::uint64_t>(coefficient)});
    }
  }
  return result;
}

std::uint64_t evaluate_at_two_two(const std::vector<TutteTerm>& terms) {
  std::uint64_t value = 0;
  for (const auto& term : terms) {
    const auto degree = term.x_degree + term.y_degree;
    REQUIRE(degree < 63);
    value += term.coefficient * (std::uint64_t{1} << degree);
  }
  return value;
}

TEST_CASE(tutte_polynomial_known_multigraphs) {
  {
    Graph graph(0, false);
    const auto result = algorithms::graphs::exact_tutte_polynomial(graph);
    REQUIRE_EQ(result.terms, (std::vector<TutteTerm>{{0, 0, 1}}));
    REQUIRE_EQ(result.logical_edge_count, std::size_t{0});
  }
  {
    Graph graph(4, false);
    const auto result = algorithms::graphs::exact_tutte_polynomial(graph);
    REQUIRE_EQ(result.terms, (std::vector<TutteTerm>{{0, 0, 1}}));
  }
  {
    Graph graph(2, false);
    graph.add_edge(0, 1, -7);
    const auto result = algorithms::graphs::exact_tutte_polynomial(graph);
    REQUIRE_EQ(result.terms, (std::vector<TutteTerm>{{1, 0, 1}}));
  }
  {
    Graph graph(1, false);
    graph.add_edge(0, 0, 99);
    const auto result = algorithms::graphs::exact_tutte_polynomial(graph);
    REQUIRE_EQ(result.terms, (std::vector<TutteTerm>{{0, 1, 1}}));
  }
  {
    Graph graph(2, false);
    graph.add_edge(0, 1, -10);
    graph.add_edge(1, 0, 123);
    const auto result = algorithms::graphs::exact_tutte_polynomial(graph);
    REQUIRE_EQ(result.terms,
               (std::vector<TutteTerm>{{0, 1, 1}, {1, 0, 1}}));
  }
  {
    Graph graph(3, false);
    graph.add_edge(0, 1);
    graph.add_edge(1, 2);
    graph.add_edge(2, 0);
    const auto result = algorithms::graphs::exact_tutte_polynomial(graph);
    REQUIRE_EQ(result.terms,
               (std::vector<TutteTerm>{{0, 1, 1}, {1, 0, 1}, {2, 0, 1}}));
  }
  {
    Graph graph(3, false);
    graph.add_edge(0, 1);
    graph.add_edge(2, 2);
    const auto result = algorithms::graphs::exact_tutte_polynomial(graph);
    REQUIRE_EQ(result.terms, (std::vector<TutteTerm>{{1, 1, 1}}));
  }
}

TEST_CASE(tutte_polynomial_validation_and_edge_limit) {
  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::exact_tutte_polynomial(directed),
                    std::invalid_argument);

  Graph exact_limit(1, false);
  for (std::size_t index = 0;
       index < algorithms::graphs::kMaxExactTutteLogicalEdges; ++index) {
    exact_limit.add_edge(0, 0, static_cast<std::int64_t>(index));
  }
  const auto at_limit = algorithms::graphs::exact_tutte_polynomial(exact_limit);
  REQUIRE_EQ(at_limit.terms,
             (std::vector<TutteTerm>{{0,
                                      algorithms::graphs::kMaxExactTutteLogicalEdges,
                                      1}}));

  Graph too_many(2, false);
  for (std::size_t index = 0;
       index <= algorithms::graphs::kMaxExactTutteLogicalEdges; ++index) {
    too_many.add_edge(0, 1, static_cast<std::int64_t>(index));
  }
  REQUIRE_THROWS_AS(algorithms::graphs::exact_tutte_polynomial(too_many),
                    std::length_error);
}

TEST_CASE(tutte_polynomial_ignores_weights_and_is_deterministic) {
  Graph first(4, false);
  first.add_edge(0, 1, -9);
  first.add_edge(1, 2, 77);
  first.add_edge(2, 0, -1);
  first.add_edge(2, 3, 6);
  first.add_edge(3, 3, -500);

  Graph second(4, false);
  second.add_edge(3, 3, 1);
  second.add_edge(3, 2, 1000);
  second.add_edge(0, 2, 4);
  second.add_edge(2, 1, 5);
  second.add_edge(1, 0, 6);

  const auto one = algorithms::graphs::exact_tutte_polynomial(first);
  const auto again = algorithms::graphs::exact_tutte_polynomial(first);
  const auto reordered = algorithms::graphs::exact_tutte_polynomial(second);
  REQUIRE_EQ(one.terms, again.terms);
  REQUIRE_EQ(one.terms, reordered.terms);
  REQUIRE_EQ(one.logical_edge_count, std::size_t{5});
  REQUIRE(one.evaluated_states > 0);
}

TEST_CASE(tutte_polynomial_randomized_subset_rank_differential) {
  std::mt19937_64 rng(0x7A77E1234ULL);
  for (std::size_t trial = 0; trial < 500; ++trial) {
    const auto vertex_count = static_cast<std::size_t>(rng() % 7U);
    const auto edge_count = vertex_count == 0 ? std::size_t{0}
                                               : static_cast<std::size_t>(rng() % 11U);
    Graph graph(vertex_count, false);
    std::vector<LogicalEdge> edges;
    edges.reserve(edge_count);
    for (std::size_t edge_index = 0; edge_index < edge_count; ++edge_index) {
      const auto first = static_cast<std::size_t>(rng() % vertex_count);
      const auto second = static_cast<std::size_t>(rng() % vertex_count);
      const auto low = std::min(first, second);
      const auto high = std::max(first, second);
      edges.emplace_back(low, high);
      const auto raw_weight = static_cast<std::int64_t>(rng() % 201U) - 100;
      if ((rng() & 1U) == 0U) {
        graph.add_edge(first, second, raw_weight);
      } else {
        graph.add_edge(second, first, raw_weight);
      }
    }

    const auto production = algorithms::graphs::exact_tutte_polynomial(graph);
    const auto oracle = subset_rank_oracle(vertex_count, edges);
    REQUIRE_EQ(production.terms, oracle);
    REQUIRE_EQ(production.logical_edge_count, edge_count);
    REQUIRE_EQ(evaluate_at_two_two(production.terms),
               std::uint64_t{1} << edge_count);
    if (oracle_connected(vertex_count, edges)) {
      constexpr std::uint64_t prime = 1'000'000'007U;
      const auto kirchhoff = algorithms::graphs::spanning_tree_count_mod_prime(
          graph, prime);
      REQUIRE_EQ(evaluate_at_one_one(production.terms) % prime, kirchhoff);
    }
  }
}

}  // namespace
