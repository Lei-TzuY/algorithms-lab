#pragma once

#include "algorithms/graphs/graphical_degree_sequence.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace graphical_degree_sequence_test_detail {

using Edge = std::pair<std::size_t, std::size_t>;
using DegreeVector = std::vector<std::size_t>;

inline std::uint64_t encode_degrees(std::span<const std::size_t> degrees) {
  const std::uint64_t base = static_cast<std::uint64_t>(degrees.size()) + 1U;
  std::uint64_t code = 0U;
  for (const std::size_t degree : degrees) {
    code = code * base + static_cast<std::uint64_t>(degree);
  }
  return code;
}

inline std::set<std::uint64_t> exhaustive_graphical_sequences(std::size_t n) {
  std::vector<Edge> possible;
  for (std::size_t u = 0; u < n; ++u) {
    for (std::size_t v = u + 1U; v < n; ++v) {
      possible.emplace_back(u, v);
    }
  }
  REQUIRE(possible.size() < 64U);
  const std::uint64_t graph_count = std::uint64_t{1} << possible.size();
  std::set<std::uint64_t> result;
  for (std::uint64_t mask = 0; mask < graph_count; ++mask) {
    DegreeVector degrees(n, 0U);
    for (std::size_t edge_index = 0; edge_index < possible.size(); ++edge_index) {
      if (((mask >> edge_index) & 1U) == 0U) {
        continue;
      }
      const auto [u, v] = possible[edge_index];
      ++degrees[u];
      ++degrees[v];
    }
    result.insert(encode_degrees(degrees));
  }
  return result;
}

inline std::uint64_t integer_power(std::uint64_t base, std::size_t exponent) {
  std::uint64_t result = 1U;
  for (std::size_t i = 0; i < exponent; ++i) {
    result *= base;
  }
  return result;
}

inline DegreeVector decode_degree_vector(std::uint64_t code, std::size_t n) {
  DegreeVector degrees(n, 0U);
  const std::uint64_t base = static_cast<std::uint64_t>(n) + 1U;
  for (std::size_t offset = 0; offset < n; ++offset) {
    const std::size_t index = n - 1U - offset;
    degrees[index] = static_cast<std::size_t>(code % base);
    code /= base;
  }
  return degrees;
}

inline void require_witness(std::span<const std::size_t> degrees,
                            const algorithms::graphs::DegreeSequenceRealization& result) {
  REQUIRE(algorithms::graphs::valid_degree_sequence_realization(degrees, result));
  std::set<Edge> unique(result.edges.begin(), result.edges.end());
  REQUIRE_EQ(unique.size(), result.edges.size());
  for (const auto& [u, v] : result.edges) {
    REQUIRE(u < v);
    REQUIRE(v < degrees.size());
  }
}

}  // namespace graphical_degree_sequence_test_detail

TEST_CASE(graphical_degree_sequence_known_and_validation) {
  using algorithms::graphs::erdos_gallai_graphical;
  using algorithms::graphs::havel_hakimi_realization;
  using graphical_degree_sequence_test_detail::require_witness;

  const std::vector<std::size_t> empty;
  REQUIRE(erdos_gallai_graphical(empty));
  const auto empty_result = havel_hakimi_realization(empty);
  REQUIRE(empty_result.has_value());
  require_witness(empty, *empty_result);

  const std::vector<std::size_t> singleton{0};
  REQUIRE(erdos_gallai_graphical(singleton));
  const auto singleton_result = havel_hakimi_realization(singleton);
  REQUIRE(singleton_result.has_value());
  require_witness(singleton, *singleton_result);

  const std::vector<std::size_t> graphical{3, 3, 2, 2, 2};
  REQUIRE(erdos_gallai_graphical(graphical));
  const auto graphical_result = havel_hakimi_realization(graphical);
  REQUIRE(graphical_result.has_value());
  require_witness(graphical, *graphical_result);

  const std::vector<std::size_t> odd_sum{2, 2, 1};
  REQUIRE(!erdos_gallai_graphical(odd_sum));
  REQUIRE(!havel_hakimi_realization(odd_sum).has_value());

  const std::vector<std::size_t> non_graphical{3, 3, 3, 1};
  REQUIRE(!erdos_gallai_graphical(non_graphical));
  REQUIRE(!havel_hakimi_realization(non_graphical).has_value());

  const std::vector<std::size_t> invalid{4, 0, 0, 0};
  REQUIRE_THROWS_AS(erdos_gallai_graphical(invalid), std::invalid_argument);
  REQUIRE_THROWS_AS(havel_hakimi_realization(invalid), std::invalid_argument);
}

TEST_CASE(havel_hakimi_witness_is_deterministic) {
  const std::vector<std::size_t> degrees{2, 2, 2, 2};
  const auto first = algorithms::graphs::havel_hakimi_realization(degrees);
  const auto second = algorithms::graphs::havel_hakimi_realization(degrees);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  REQUIRE_EQ(*first, *second);
  graphical_degree_sequence_test_detail::require_witness(degrees, *first);
  const std::vector<graphical_degree_sequence_test_detail::Edge> expected{
      {0, 1}, {0, 2}, {1, 3}, {2, 3}};
  REQUIRE_EQ(first->edges, expected);
}

TEST_CASE(graphical_degree_sequence_exhaustive_simple_graph_oracle) {
  using graphical_degree_sequence_test_detail::decode_degree_vector;
  using graphical_degree_sequence_test_detail::encode_degrees;
  using graphical_degree_sequence_test_detail::exhaustive_graphical_sequences;
  using graphical_degree_sequence_test_detail::integer_power;
  using graphical_degree_sequence_test_detail::require_witness;

  for (std::size_t n = 0; n <= 6U; ++n) {
    const auto oracle = exhaustive_graphical_sequences(n);
    const std::uint64_t count = integer_power(static_cast<std::uint64_t>(n) + 1U, n);
    for (std::uint64_t code = 0; code < count; ++code) {
      const auto degrees = decode_degree_vector(code, n);
      bool domain_valid = true;
      for (const std::size_t degree : degrees) {
        if (degree >= n && n != 0U) {
          domain_valid = false;
          break;
        }
      }
      if (!domain_valid) {
        continue;
      }
      const bool expected = oracle.contains(encode_degrees(degrees));
      const bool eg = algorithms::graphs::erdos_gallai_graphical(degrees);
      const auto hh = algorithms::graphs::havel_hakimi_realization(degrees);
      REQUIRE_EQ(eg, expected);
      REQUIRE_EQ(hh.has_value(), expected);
      if (hh.has_value()) {
        require_witness(degrees, *hh);
      }
    }
  }
}

TEST_CASE(graphical_degree_sequence_randomized_cross_formulation) {
  std::mt19937_64 rng(0xD36E'5EEDULL);
  for (std::size_t trial = 0; trial < 1200U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 41U);
    std::vector<std::size_t> degrees(n, 0U);
    if (n != 0U) {
      for (auto& degree : degrees) {
        degree = static_cast<std::size_t>(rng() % n);
      }
    }
    const bool eg = algorithms::graphs::erdos_gallai_graphical(degrees);
    const auto hh = algorithms::graphs::havel_hakimi_realization(degrees);
    REQUIRE_EQ(hh.has_value(), eg);
    if (hh.has_value()) {
      graphical_degree_sequence_test_detail::require_witness(degrees, *hh);
    }
  }

  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 50U);
    std::vector<std::size_t> degrees(n, 0U);
    for (std::size_t u = 0; u < n; ++u) {
      for (std::size_t v = u + 1U; v < n; ++v) {
        if ((rng() & 7U) < 2U) {
          ++degrees[u];
          ++degrees[v];
        }
      }
    }
    REQUIRE(algorithms::graphs::erdos_gallai_graphical(degrees));
    const auto hh = algorithms::graphs::havel_hakimi_realization(degrees);
    REQUIRE(hh.has_value());
    graphical_degree_sequence_test_detail::require_witness(degrees, *hh);
  }
}
