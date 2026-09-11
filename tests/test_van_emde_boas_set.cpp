#include "algorithms/data_structures/van_emde_boas_set.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

using algorithms::data_structures::VanEmdeBoasSet;

static std::optional<std::uint32_t> ref_succ(const std::set<std::uint32_t>& s,
                                              std::uint32_t x) {
  const auto it = s.upper_bound(x);
  return it == s.end() ? std::nullopt
                       : std::optional<std::uint32_t>(*it);
}

static std::optional<std::uint32_t> ref_pred(const std::set<std::uint32_t>& s,
                                              std::uint32_t x) {
  auto it = s.lower_bound(x);
  if (it == s.begin()) {
    return std::nullopt;
  }
  --it;
  return *it;
}

static std::optional<std::uint32_t> ref_min(const std::set<std::uint32_t>& s) {
  return s.empty() ? std::nullopt
                   : std::optional<std::uint32_t>(*s.begin());
}

static std::optional<std::uint32_t> ref_max(const std::set<std::uint32_t>& s) {
  return s.empty() ? std::nullopt
                   : std::optional<std::uint32_t>(*s.rbegin());
}

TEST_CASE(van_emde_boas_validates_universe_and_key_bounds) {
  REQUIRE_THROWS_AS(VanEmdeBoasSet(0U), std::invalid_argument);
  REQUIRE_THROWS_AS(VanEmdeBoasSet(17U), std::invalid_argument);

  VanEmdeBoasSet set(4U);
  REQUIRE_EQ(set.universe_size(), 16U);
  REQUIRE_THROWS_AS(set.insert(16U), std::out_of_range);
  REQUIRE_THROWS_AS(set.erase(16U), std::out_of_range);
  REQUIRE_THROWS_AS(set.contains(16U), std::out_of_range);
  REQUIRE_THROWS_AS(set.predecessor(16U), std::out_of_range);
  REQUIRE_THROWS_AS(set.successor(16U), std::out_of_range);
}

TEST_CASE(van_emde_boas_base_universe_and_duplicates) {
  VanEmdeBoasSet set(1U);
  REQUIRE(set.empty());
  REQUIRE(set.insert(1U));
  REQUIRE(set.insert(0U));
  REQUIRE(!set.insert(1U));
  REQUIRE_EQ(set.minimum(), std::optional<std::uint32_t>(0U));
  REQUIRE_EQ(set.maximum(), std::optional<std::uint32_t>(1U));
  REQUIRE_EQ(set.successor(0U), std::optional<std::uint32_t>(1U));
  REQUIRE_EQ(set.predecessor(1U), std::optional<std::uint32_t>(0U));
  REQUIRE(set.erase(0U));
  REQUIRE(!set.erase(0U));
  REQUIRE_EQ(set.minimum(), std::optional<std::uint32_t>(1U));
  REQUIRE(set.erase(1U));
  REQUIRE(set.empty());
}

TEST_CASE(van_emde_boas_full_boundary_and_delete_min_replacement) {
  VanEmdeBoasSet set(16U);
  for (const auto value : std::vector<std::uint32_t>{
           0U, 1U, 255U, 256U, 257U, 65534U, 65535U}) {
    REQUIRE(set.insert(value));
  }
  REQUIRE_EQ(set.successor(255U), std::optional<std::uint32_t>(256U));
  REQUIRE_EQ(set.predecessor(256U), std::optional<std::uint32_t>(255U));
  REQUIRE_EQ(set.maximum(), std::optional<std::uint32_t>(65535U));
  REQUIRE(set.erase(0U));
  REQUIRE_EQ(set.minimum(), std::optional<std::uint32_t>(1U));
  REQUIRE(set.erase(1U));
  REQUIRE_EQ(set.minimum(), std::optional<std::uint32_t>(255U));
  REQUIRE(set.erase(65535U));
  REQUIRE_EQ(set.maximum(), std::optional<std::uint32_t>(65534U));
}

TEST_CASE(van_emde_boas_randomized_differential_against_std_set) {
  VanEmdeBoasSet set(8U);
  std::set<std::uint32_t> reference;
  std::mt19937 rng(0x5EEDB055U);
  std::uniform_int_distribution<unsigned> key_dist(0U, 255U);
  std::uniform_int_distribution<unsigned> op_dist(0U, 5U);

  for (std::size_t step = 0; step < 20000U; ++step) {
    const auto key = static_cast<std::uint32_t>(key_dist(rng));
    const auto op = op_dist(rng);
    if (op == 0U) {
      REQUIRE_EQ(set.insert(key), reference.insert(key).second);
    } else if (op == 1U) {
      REQUIRE_EQ(set.erase(key), reference.erase(key) == 1U);
    } else if (op == 2U) {
      REQUIRE_EQ(set.contains(key), reference.contains(key));
    } else if (op == 3U) {
      REQUIRE_EQ(set.successor(key), ref_succ(reference, key));
    } else if (op == 4U) {
      REQUIRE_EQ(set.predecessor(key), ref_pred(reference, key));
    } else {
      REQUIRE_EQ(set.minimum(), ref_min(reference));
      REQUIRE_EQ(set.maximum(), ref_max(reference));
    }

    REQUIRE_EQ(set.size(), reference.size());
    REQUIRE_EQ(set.empty(), reference.empty());
    REQUIRE_EQ(set.minimum(), ref_min(reference));
    REQUIRE_EQ(set.maximum(), ref_max(reference));

    if ((step % 97U) == 0U) {
      for (std::uint32_t query = 0; query < 256U; query += 17U) {
        REQUIRE_EQ(set.contains(query), reference.contains(query));
        REQUIRE_EQ(set.successor(query), ref_succ(reference, query));
        REQUIRE_EQ(set.predecessor(query), ref_pred(reference, query));
      }
    }
  }
}
