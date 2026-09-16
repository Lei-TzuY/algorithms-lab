#pragma once

#include "algorithms/combinatorial/schreier_sims.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::combinatorial::Permutation;
using algorithms::combinatorial::SchreierSimsGroup;

Permutation independent_identity(std::size_t degree) {
  Permutation permutation(degree);
  std::iota(permutation.begin(), permutation.end(), std::size_t{0});
  return permutation;
}

Permutation independent_compose(const Permutation& left,
                                const Permutation& right) {
  Permutation result(left.size());
  for (std::size_t index = 0; index < left.size(); ++index) {
    result[index] = left[right[index]];
  }
  return result;
}

std::set<Permutation> independent_group_closure(
    std::size_t degree, const std::vector<Permutation>& generators) {
  std::set<Permutation> closure;
  std::queue<Permutation> pending;
  const Permutation identity = independent_identity(degree);
  closure.insert(identity);
  pending.push(identity);

  while (!pending.empty()) {
    const Permutation current = pending.front();
    pending.pop();
    for (const auto& generator : generators) {
      Permutation next = independent_compose(generator, current);
      if (closure.insert(next).second) {
        pending.push(std::move(next));
      }
    }
  }
  return closure;
}

Permutation independent_cycle(std::size_t degree,
                              std::initializer_list<std::size_t> points) {
  Permutation result = independent_identity(degree);
  const std::vector<std::size_t> cycle(points);
  if (cycle.empty()) {
    return result;
  }
  for (std::size_t index = 0; index < cycle.size(); ++index) {
    result[cycle[index]] = cycle[(index + 1U) % cycle.size()];
  }
  return result;
}

std::vector<Permutation> independent_all_permutations(std::size_t degree) {
  std::vector<Permutation> permutations;
  Permutation permutation = independent_identity(degree);
  do {
    permutations.push_back(permutation);
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return permutations;
}

void verify_failure_replay(const SchreierSimsGroup& group,
                           const Permutation& candidate) {
  const auto result = group.sift(candidate);
  REQUIRE(!result.member);
  REQUIRE(result.first_failed_level < group.levels().size());

  for (std::size_t level_index = 0; level_index < result.first_failed_level;
       ++level_index) {
    const std::size_t base = group.levels()[level_index].base_point;
    REQUIRE_EQ(result.residue[base], base);
  }

  const auto& failed = group.levels()[result.first_failed_level];
  const std::size_t image = result.residue[failed.base_point];
  REQUIRE(std::find(failed.orbit.begin(), failed.orbit.end(), image) ==
          failed.orbit.end());
}

}  // namespace

TEST_CASE(schreier_sims_known_groups_validation_and_diagnostics) {
  {
    SchreierSimsGroup trivial(0, {});
    REQUIRE_EQ(trivial.order_u64(), std::uint64_t{1});
    REQUIRE(trivial.levels().empty());
    REQUIRE(trivial.contains(Permutation{}));
  }

  {
    const Permutation generator = independent_cycle(5, {0, 1, 2, 3, 4});
    SchreierSimsGroup cyclic(5, {generator});
    REQUIRE_EQ(cyclic.order_u64(), std::uint64_t{5});
    REQUIRE_EQ(cyclic.order_factors(),
               std::vector<std::size_t>({5, 1, 1, 1, 1}));
    const auto closure = independent_group_closure(5, {generator});
    for (const auto& permutation : independent_all_permutations(5)) {
      REQUIRE_EQ(cyclic.contains(permutation), closure.contains(permutation));
    }
  }

  {
    const Permutation rotation{1, 2, 3, 0};
    const Permutation reflection{1, 0, 3, 2};
    SchreierSimsGroup dihedral(4, {rotation, reflection});
    REQUIRE_EQ(dihedral.order_u64(), std::uint64_t{8});
  }

  {
    const Permutation transposition{1, 0, 2, 3};
    const Permutation cycle4{1, 2, 3, 0};
    SchreierSimsGroup symmetric4(4, {transposition, cycle4});
    REQUIRE_EQ(symmetric4.order_u64(), std::uint64_t{24});
    for (const auto& level : symmetric4.levels()) {
      REQUIRE(std::is_sorted(level.orbit.begin(), level.orbit.end()));
      REQUIRE(std::find(level.orbit.begin(), level.orbit.end(),
                        level.base_point) != level.orbit.end());
    }
  }

  REQUIRE_THROWS_AS(SchreierSimsGroup(3, {Permutation{0, 0, 2}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(SchreierSimsGroup(3, {Permutation{0, 1}}),
                    std::invalid_argument);
}

TEST_CASE(schreier_sims_membership_residue_is_replayable) {
  const Permutation generator = independent_cycle(5, {0, 1, 2, 3, 4});
  SchreierSimsGroup cyclic(5, {generator});

  const Permutation member{2, 3, 4, 0, 1};
  const auto member_result = cyclic.sift(member);
  REQUIRE(member_result.member);
  REQUIRE_EQ(member_result.first_failed_level, cyclic.levels().size());
  REQUIRE_EQ(member_result.residue, independent_identity(5));

  const Permutation nonmember{1, 0, 2, 3, 4};
  verify_failure_replay(cyclic, nonmember);

  REQUIRE_THROWS_AS(cyclic.contains(Permutation{0, 1, 2, 4, 4}),
                    std::invalid_argument);
}

TEST_CASE(schreier_sims_exhaustive_random_small_group_differential) {
  std::mt19937_64 rng(0x5343485245494552ULL);
  for (std::size_t trial = 0; trial < 180; ++trial) {
    const std::size_t degree = static_cast<std::size_t>(rng() % 7U);
    const auto all = independent_all_permutations(degree);
    const std::size_t generator_count =
        degree == 0 ? 0 : static_cast<std::size_t>(rng() % 4U);

    std::vector<Permutation> generators;
    generators.reserve(generator_count);
    for (std::size_t index = 0; index < generator_count; ++index) {
      generators.push_back(
          all[static_cast<std::size_t>(rng() % all.size())]);
    }

    const auto closure = independent_group_closure(degree, generators);
    SchreierSimsGroup group(degree, generators);
    REQUIRE_EQ(group.order_u64(), static_cast<std::uint64_t>(closure.size()));

    for (const auto& permutation : all) {
      const auto membership = group.sift(permutation);
      REQUIRE_EQ(membership.member, closure.contains(permutation));
      if (membership.member) {
        REQUIRE_EQ(membership.first_failed_level, degree);
        REQUIRE_EQ(membership.residue, independent_identity(degree));
      } else {
        verify_failure_replay(group, permutation);
      }
    }

    std::reverse(generators.begin(), generators.end());
    SchreierSimsGroup reordered(degree, generators);
    REQUIRE_EQ(reordered.order_factors(), group.order_factors());
    REQUIRE_EQ(reordered.order_u64(), group.order_u64());
  }
}

TEST_CASE(schreier_sims_exact_factors_survive_u64_order_overflow) {
  constexpr std::size_t degree = 21;
  Permutation transposition = independent_identity(degree);
  std::swap(transposition[0], transposition[1]);
  Permutation long_cycle(degree);
  for (std::size_t index = 0; index < degree; ++index) {
    long_cycle[index] = (index + 1U) % degree;
  }

  SchreierSimsGroup symmetric21(degree, {transposition, long_cycle});
  std::vector<std::size_t> expected;
  expected.reserve(degree);
  for (std::size_t factor = degree; factor != 0; --factor) {
    expected.push_back(factor);
  }
  REQUIRE_EQ(symmetric21.order_factors(), expected);
  REQUIRE_THROWS_AS(symmetric21.order_u64(), std::overflow_error);
}
