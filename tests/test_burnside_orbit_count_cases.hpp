#pragma once

#include "algorithms/combinatorial/burnside_orbit_count.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace {

using Permutation = std::vector<std::size_t>;

std::vector<Permutation> cyclic_group(std::size_t n) {
  std::vector<Permutation> group;
  group.reserve(n);
  for (std::size_t shift = 0; shift < n; ++shift) {
    Permutation p(n);
    for (std::size_t i = 0; i < n; ++i) {
      p[i] = (i + shift) % n;
    }
    group.push_back(std::move(p));
  }
  return group;
}

std::vector<Permutation> dihedral_group(std::size_t n) {
  auto group = cyclic_group(n);
  group.reserve(2 * n);
  for (std::size_t shift = 0; shift < n; ++shift) {
    Permutation p(n);
    for (std::size_t i = 0; i < n; ++i) {
      p[i] = (shift + n - i) % n;
    }
    group.push_back(std::move(p));
  }
  return group;
}

std::vector<std::uint8_t> transform_coloring(
    const std::vector<std::uint8_t>& coloring, const Permutation& permutation) {
  std::vector<std::uint8_t> transformed(coloring.size());
  for (std::size_t i = 0; i < coloring.size(); ++i) {
    transformed[permutation[i]] = coloring[i];
  }
  return transformed;
}

std::uint64_t exhaustive_orbit_count(
    std::size_t n, std::uint8_t colors,
    const std::vector<Permutation>& group) {
  std::uint64_t total_colorings = 1;
  for (std::size_t i = 0; i < n; ++i) {
    total_colorings *= colors;
  }

  std::set<std::vector<std::uint8_t>> visited;
  std::uint64_t orbits = 0;
  for (std::uint64_t code = 0; code < total_colorings; ++code) {
    std::uint64_t value = code;
    std::vector<std::uint8_t> coloring(n);
    for (std::size_t i = 0; i < n; ++i) {
      coloring[i] = static_cast<std::uint8_t>(value % colors);
      value /= colors;
    }
    if (visited.contains(coloring)) {
      continue;
    }
    ++orbits;
    for (const auto& permutation : group) {
      visited.insert(transform_coloring(coloring, permutation));
    }
  }
  return orbits;
}

TEST_CASE(burnside_known_cyclic_and_dihedral_counts) {
  using algorithms::combinatorial::count_color_orbits_burnside;

  const auto c3 = cyclic_group(3);
  const auto necklace = count_color_orbits_burnside(3, 2, c3);
  REQUIRE_EQ(necklace.orbit_count, std::uint64_t{4});
  REQUIRE_EQ(necklace.fixed_coloring_sum, std::uint64_t{12});
  REQUIRE_EQ(necklace.elements.size(), std::size_t{3});
  REQUIRE_EQ(necklace.elements[0].cycle_count, std::size_t{3});
  REQUIRE_EQ(necklace.elements[0].fixed_colorings, std::uint64_t{8});
  REQUIRE_EQ(necklace.elements[1].cycle_count, std::size_t{1});
  REQUIRE_EQ(necklace.elements[2].cycle_count, std::size_t{1});

  const auto d4 = dihedral_group(4);
  const auto bracelet = count_color_orbits_burnside(4, 2, d4);
  REQUIRE_EQ(bracelet.orbit_count, std::uint64_t{6});
  REQUIRE_EQ(bracelet.fixed_coloring_sum, std::uint64_t{48});
}

TEST_CASE(burnside_trivial_and_empty_position_domain) {
  using algorithms::combinatorial::count_color_orbits_burnside;

  const std::vector<Permutation> trivial4{{0, 1, 2, 3}};
  const auto unrestricted = count_color_orbits_burnside(4, 3, trivial4);
  REQUIRE_EQ(unrestricted.orbit_count, std::uint64_t{81});

  const std::vector<Permutation> empty_identity{{}};
  const auto empty = count_color_orbits_burnside(0, 7, empty_identity);
  REQUIRE_EQ(empty.orbit_count, std::uint64_t{1});
  REQUIRE_EQ(empty.elements[0].cycle_count, std::size_t{0});
}

TEST_CASE(burnside_rejects_invalid_group_descriptions) {
  using algorithms::combinatorial::count_color_orbits_burnside;

  REQUIRE_THROWS_AS(count_color_orbits_burnside(2, 0, {{0, 1}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(count_color_orbits_burnside(2, 2, {}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(count_color_orbits_burnside(2, 2, {{0, 1}, {0}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(count_color_orbits_burnside(2, 2, {{0, 1}, {0, 0}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(count_color_orbits_burnside(2, 2, {{0, 1}, {0, 1}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(count_color_orbits_burnside(2, 2, {{1, 0}}),
                    std::invalid_argument);

  const std::vector<Permutation> non_closed{{0, 1, 2}, {1, 0, 2}, {0, 2, 1}};
  REQUIRE_THROWS_AS(count_color_orbits_burnside(3, 2, non_closed),
                    std::invalid_argument);
}

TEST_CASE(burnside_reports_representability_overflow) {
  using algorithms::combinatorial::count_color_orbits_burnside;
  const std::vector<Permutation> identity2{{0, 1}};
  REQUIRE_THROWS_AS(
      count_color_orbits_burnside(
          2, std::numeric_limits<std::uint64_t>::max(), identity2),
      std::overflow_error);
}

TEST_CASE(burnside_randomized_differential_against_explicit_orbits) {
  using algorithms::combinatorial::count_color_orbits_burnside;

  std::mt19937_64 rng(0xB17D51DEULL);
  for (std::size_t trial = 0; trial < 300; ++trial) {
    const std::size_t n = 1 + static_cast<std::size_t>(rng() % 6);
    const auto colors = static_cast<std::uint8_t>(1 + (rng() % 3));
    const bool use_dihedral = n >= 3 && (rng() & 1ULL) != 0;
    const auto group = use_dihedral ? dihedral_group(n) : cyclic_group(n);

    const auto actual = count_color_orbits_burnside(n, colors, group);
    const auto expected = exhaustive_orbit_count(n, colors, group);
    REQUIRE_EQ(actual.orbit_count, expected);
    REQUIRE_EQ(actual.elements.size(), group.size());
  }
}

}  // namespace
