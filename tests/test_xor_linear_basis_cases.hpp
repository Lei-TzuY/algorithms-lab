#pragma once

#include "algorithms/data_structures/xor_linear_basis.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <vector>

namespace {

std::set<std::uint64_t> xor_basis_exhaustive_span(
    const std::vector<std::uint64_t>& values) {
  std::set<std::uint64_t> span{0};
  for (const auto value : values) {
    std::vector<std::uint64_t> additions;
    additions.reserve(span.size());
    for (const auto x : span) {
      additions.push_back(x ^ value);
    }
    span.insert(additions.begin(), additions.end());
  }
  return span;
}

}  // namespace

TEST_CASE(xor_basis_deterministic_full_width_and_dependence) {
  algorithms::data_structures::XorLinearBasis64 basis;
  REQUIRE(basis.empty());
  REQUIRE(basis.valid_structure());
  REQUIRE(basis.contains(0));
  REQUIRE_EQ(basis.maximize_xor(), std::uint64_t{0});
  REQUIRE(!basis.insert(0));
  REQUIRE(basis.insert(std::uint64_t{1} << 63));
  REQUIRE(basis.insert(5));
  REQUIRE(!basis.insert((std::uint64_t{1} << 63) ^ 5));
  REQUIRE_EQ(basis.rank(), std::size_t{2});
  REQUIRE(basis.contains((std::uint64_t{1} << 63) ^ 5));
  REQUIRE(!basis.contains(2));
  REQUIRE_EQ(basis.maximize_xor(), (std::uint64_t{1} << 63) ^ 5);
  REQUIRE_EQ(basis.maximize_xor(2), (std::uint64_t{1} << 63) ^ 7);
  REQUIRE(basis.valid_structure());
}

TEST_CASE(xor_basis_canonical_form_is_insertion_order_independent) {
  const std::vector<std::uint64_t> values = {
      0xB3, 0x4D, 0xF0, 0x19, 0xB3 ^ 0x4D, 0};
  algorithms::data_structures::XorLinearBasis64 forward;
  algorithms::data_structures::XorLinearBasis64 reverse;
  for (const auto value : values) {
    static_cast<void>(forward.insert(value));
  }
  for (auto it = values.rbegin(); it != values.rend(); ++it) {
    static_cast<void>(reverse.insert(*it));
  }
  REQUIRE_EQ(forward.rank(), reverse.rank());
  REQUIRE_EQ(forward.canonical_basis(), reverse.canonical_basis());
  REQUIRE(forward.valid_structure());
  REQUIRE(reverse.valid_structure());
}

TEST_CASE(xor_basis_exhaustive_randomized_span_and_maximization) {
  std::mt19937_64 rng(0x584F524241534953ULL);
  for (std::size_t trial = 0; trial < 1200; ++trial) {
    const std::size_t count = static_cast<std::size_t>(rng() % 11);
    std::vector<std::uint64_t> values;
    values.reserve(count);
    algorithms::data_structures::XorLinearBasis64 basis;
    for (std::size_t i = 0; i < count; ++i) {
      const std::uint64_t value = rng() & 0xFFFFU;
      values.push_back(value);
      static_cast<void>(basis.insert(value));
      REQUIRE(basis.valid_structure());
    }

    const auto span = xor_basis_exhaustive_span(values);
    std::size_t expected_rank = 0;
    std::size_t span_size = span.size();
    while (span_size > 1) {
      span_size >>= 1;
      ++expected_rank;
    }
    REQUIRE_EQ(basis.rank(), expected_rank);

    for (std::uint64_t query = 0; query < 256; ++query) {
      REQUIRE_EQ(basis.contains(query), span.contains(query));
    }

    const std::uint64_t seed = rng() & 0xFFFFU;
    std::uint64_t best = seed;
    for (const auto x : span) {
      best = std::max(best, seed ^ x);
    }
    REQUIRE_EQ(basis.maximize_xor(seed), best);
  }
}

TEST_CASE(xor_basis_randomized_canonical_replay) {
  std::mt19937_64 rng(0x0C0FFEE123456789ULL);
  for (std::size_t trial = 0; trial < 500; ++trial) {
    std::vector<std::uint64_t> values;
    const std::size_t count = static_cast<std::size_t>(rng() % 64);
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      values.push_back(rng());
    }

    algorithms::data_structures::XorLinearBasis64 baseline;
    for (const auto value : values) {
      static_cast<void>(baseline.insert(value));
    }
    const auto expected = baseline.canonical_basis();

    std::shuffle(values.begin(), values.end(), rng);
    algorithms::data_structures::XorLinearBasis64 shuffled;
    for (const auto value : values) {
      static_cast<void>(shuffled.insert(value));
    }
    REQUIRE_EQ(shuffled.canonical_basis(), expected);
    REQUIRE(shuffled.valid_structure());
  }
}
