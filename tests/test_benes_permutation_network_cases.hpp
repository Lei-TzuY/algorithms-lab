#pragma once

#include "algorithms/combinatorial/benes_permutation_network.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using algorithms::combinatorial::BenesPermutationRoute;
using algorithms::combinatorial::benes_apply;
using algorithms::combinatorial::benes_route_permutation;
using algorithms::combinatorial::benes_switch_count;
using algorithms::combinatorial::benes_switch_settings_preorder;
using algorithms::combinatorial::valid_benes_permutation_route;

namespace benes_permutation_network_test_detail {

template <class T>
inline std::vector<T> direct_permute(
    const std::vector<std::size_t>& permutation,
    const std::vector<T>& inputs) {
  std::vector<T> outputs = inputs;
  for (std::size_t input = 0U;
       input < permutation.size(); ++input) {
    outputs[permutation[input]] = inputs[input];
  }
  return outputs;
}

// Independent interpreter of the public recursive route representation.
// This intentionally does not call benes_apply().
template <class T>
inline std::vector<T> independent_apply(
    const BenesPermutationRoute& route,
    const std::vector<T>& inputs) {
  if (route.width <= 1U) {
    return inputs;
  }
  if (route.width == 2U) {
    if (!route.leaf_cross) {
      return inputs;
    }
    return std::vector<T>{inputs[1U], inputs[0U]};
  }

  const std::size_t half = route.width / 2U;
  std::vector<T> upper;
  std::vector<T> lower;
  upper.reserve(half);
  lower.reserve(half);

  for (std::size_t index = 0U; index < half; ++index) {
    const std::size_t first = 2U * index;
    const std::size_t second = first + 1U;
    if (route.input_cross[index] == 0U) {
      upper.push_back(inputs[first]);
      lower.push_back(inputs[second]);
    } else {
      upper.push_back(inputs[second]);
      lower.push_back(inputs[first]);
    }
  }

  upper = independent_apply(*route.upper, upper);
  lower = independent_apply(*route.lower, lower);

  std::vector<T> outputs = inputs;
  for (std::size_t index = 0U; index < half; ++index) {
    const std::size_t first = 2U * index;
    const std::size_t second = first + 1U;
    if (route.output_cross[index] == 0U) {
      outputs[first] = upper[index];
      outputs[second] = lower[index];
    } else {
      outputs[first] = lower[index];
      outputs[second] = upper[index];
    }
  }
  return outputs;
}

inline std::size_t expected_switch_count(const std::size_t n) {
  if (n <= 1U) {
    return 0U;
  }
  std::size_t levels = 0U;
  for (std::size_t width = n; width > 1U; width /= 2U) {
    ++levels;
  }
  return (n / 2U) * (2U * levels - 1U);
}

inline void require_routes(
    const std::vector<std::size_t>& permutation) {
  const std::size_t n = permutation.size();
  const BenesPermutationRoute route =
      benes_route_permutation(permutation);

  REQUIRE(valid_benes_permutation_route(route));
  REQUIRE_EQ(route.width, n);
  REQUIRE_EQ(benes_switch_count(route),
             expected_switch_count(n));

  const auto settings = benes_switch_settings_preorder(route);
  REQUIRE_EQ(settings.size(), expected_switch_count(n));
  for (const std::uint8_t setting : settings) {
    REQUIRE(setting <= 1U);
  }

  std::vector<std::uint64_t> payload(n);
  for (std::size_t index = 0U; index < n; ++index) {
    payload[index] =
        UINT64_C(0x9E3779B97F4A7C15) ^
        static_cast<std::uint64_t>(index * 131U + 17U);
  }

  const auto expected = direct_permute(permutation, payload);
  REQUIRE_EQ(benes_apply(route, payload), expected);
  REQUIRE_EQ(independent_apply(route, payload), expected);
}

}  // namespace benes_permutation_network_test_detail

TEST_CASE(benes_permutation_network_trivial_and_invalid_contracts) {
  using namespace benes_permutation_network_test_detail;

  require_routes({});
  require_routes({0U});
  require_routes({0U, 1U});
  require_routes({1U, 0U});

  REQUIRE_THROWS_AS(
      benes_route_permutation({0U, 1U, 2U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      benes_route_permutation({0U, 0U, 2U, 3U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      benes_route_permutation({0U, 1U, 2U, 4U}),
      std::invalid_argument);

  const auto route = benes_route_permutation({1U, 0U});
  REQUIRE_THROWS_AS(
      benes_apply(route, std::vector<int>{7}),
      std::invalid_argument);

  BenesPermutationRoute malformed = route;
  malformed.width = 4U;
  REQUIRE(!valid_benes_permutation_route(malformed));
  REQUIRE_THROWS_AS(
      benes_switch_count(malformed),
      std::invalid_argument);
}

TEST_CASE(benes_permutation_network_routes_generic_payloads) {
  const std::vector<std::size_t> permutation{
      5U, 2U, 7U, 0U, 3U, 6U, 1U, 4U};
  const auto route = benes_route_permutation(permutation);

  const std::vector<std::string> payload{
      "zero", "one", "two", "three",
      "four", "five", "six", "seven"};

  REQUIRE_EQ(
      benes_apply(route, payload),
      benes_permutation_network_test_detail::direct_permute(
          permutation, payload));

  REQUIRE_EQ(
      benes_switch_settings_preorder(route),
      benes_switch_settings_preorder(
          benes_route_permutation(permutation)));
}

TEST_CASE(benes_permutation_network_exhausts_all_eight_wire_permutations) {
  using namespace benes_permutation_network_test_detail;

  std::vector<std::size_t> permutation(8U);
  std::iota(permutation.begin(), permutation.end(), 0U);

  std::size_t checked = 0U;
  do {
    require_routes(permutation);
    ++checked;
  } while (std::next_permutation(
      permutation.begin(), permutation.end()));

  REQUIRE_EQ(checked, 40320U);
}

TEST_CASE(benes_permutation_network_random_larger_routes) {
  using namespace benes_permutation_network_test_detail;

  std::mt19937_64 random(UINT64_C(0xB3E35A17C0DE));
  for (const std::size_t n : {16U, 32U, 64U}) {
    std::vector<std::size_t> permutation(n);
    std::iota(permutation.begin(), permutation.end(), 0U);

    for (std::size_t trial = 0U; trial < 350U; ++trial) {
      std::shuffle(permutation.begin(), permutation.end(), random);
      require_routes(permutation);
    }
  }
}
