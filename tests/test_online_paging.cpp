#include "algorithms/online/paging.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using algorithms::online::Page;
using algorithms::online::PagingEvent;
using algorithms::online::PagingResult;
using algorithms::online::belady_optimal_paging;
using algorithms::online::lru_paging;

struct StateKey {
  std::size_t index{};
  std::vector<Page> cache;

  friend bool operator<(const StateKey& lhs, const StateKey& rhs) {
    return std::tie(lhs.index, lhs.cache) < std::tie(rhs.index, rhs.cache);
  }
};

std::size_t exact_offline_faults(std::span<const Page> requests,
                                 std::size_t capacity) {
  std::map<StateKey, std::size_t> memo;
  std::function<std::size_t(std::size_t, std::vector<Page>)> solve =
      [&](std::size_t index, std::vector<Page> cache) -> std::size_t {
    if (index == requests.size()) {
      return 0;
    }
    std::sort(cache.begin(), cache.end());
    const StateKey key{index, cache};
    if (const auto found = memo.find(key); found != memo.end()) {
      return found->second;
    }

    const Page page = requests[index];
    if (std::binary_search(cache.begin(), cache.end(), page)) {
      const std::size_t value = solve(index + 1U, cache);
      memo.emplace(key, value);
      return value;
    }

    if (capacity == 0U) {
      const std::size_t value = 1U + solve(index + 1U, {});
      memo.emplace(key, value);
      return value;
    }

    std::size_t best = std::numeric_limits<std::size_t>::max();
    if (cache.size() < capacity) {
      cache.push_back(page);
      best = 1U + solve(index + 1U, std::move(cache));
    } else {
      for (std::size_t victim = 0; victim < cache.size(); ++victim) {
        auto next_cache = cache;
        next_cache[victim] = page;
        best = std::min(best, 1U + solve(index + 1U, std::move(next_cache)));
      }
    }
    memo.emplace(key, best);
    return best;
  };

  return solve(0U, {});
}

void verify_generic_trace(std::span<const Page> requests,
                          const PagingResult& result) {
  REQUIRE_EQ(result.events.size(), requests.size());
  std::set<Page> cache;
  std::size_t faults = 0;

  for (std::size_t index = 0; index < requests.size(); ++index) {
    const PagingEvent& event = result.events[index];
    REQUIRE_EQ(event.page, requests[index]);
    const bool resident_before = cache.contains(event.page);
    REQUIRE_EQ(event.hit, resident_before);

    if (event.hit) {
      REQUIRE(!event.evicted.has_value());
      continue;
    }

    ++faults;
    if (event.evicted.has_value()) {
      REQUIRE(cache.contains(*event.evicted));
      cache.erase(*event.evicted);
    }
    if (result.capacity != 0U) {
      cache.insert(event.page);
    }
    REQUIRE(cache.size() <= result.capacity);
  }

  REQUIRE_EQ(faults, result.faults);
}

void verify_lru_policy(std::span<const Page> requests,
                       const PagingResult& result) {
  std::vector<Page> recency;  // MRU first.
  REQUIRE_EQ(result.events.size(), requests.size());

  for (std::size_t index = 0; index < requests.size(); ++index) {
    const Page page = requests[index];
    const PagingEvent& event = result.events[index];
    const auto found = std::find(recency.begin(), recency.end(), page);
    const bool expected_hit = found != recency.end();
    REQUIRE_EQ(event.hit, expected_hit);

    if (expected_hit) {
      REQUIRE(!event.evicted.has_value());
      recency.erase(found);
      recency.insert(recency.begin(), page);
      continue;
    }

    if (result.capacity == 0U) {
      REQUIRE(!event.evicted.has_value());
      continue;
    }

    if (recency.size() == result.capacity) {
      REQUIRE_EQ(event.evicted, std::optional<Page>{recency.back()});
      recency.pop_back();
    } else {
      REQUIRE(!event.evicted.has_value());
    }
    recency.insert(recency.begin(), page);
  }
}

void check_against_exact(std::span<const Page> requests, std::size_t capacity) {
  const PagingResult lru = lru_paging(requests, capacity);
  const PagingResult offline = belady_optimal_paging(requests, capacity);
  const std::size_t exact = exact_offline_faults(requests, capacity);

  verify_generic_trace(requests, lru);
  verify_lru_policy(requests, lru);
  verify_generic_trace(requests, offline);
  REQUIRE_EQ(offline.faults, exact);
  REQUIRE(lru.faults >= exact);
  if (capacity > 0U) {
    REQUIRE(lru.faults <= capacity * exact + capacity);
  }
}

void enumerate_sequences(std::size_t position, std::vector<Page>& sequence,
                         std::size_t alphabet_size, std::size_t capacity) {
  if (position == sequence.size()) {
    check_against_exact(sequence, capacity);
    return;
  }
  for (std::size_t value = 0; value < alphabet_size; ++value) {
    sequence[position] = static_cast<Page>(value);
    enumerate_sequences(position + 1U, sequence, alphabet_size, capacity);
  }
}

}  // namespace

TEST_CASE(paging_empty_zero_capacity_and_extreme_pages) {
  const std::vector<Page> empty;
  REQUIRE_EQ(lru_paging(empty, 3U).faults, 0U);
  REQUIRE_EQ(belady_optimal_paging(empty, 3U).faults, 0U);

  const std::vector<Page> requests{1, 2, 3};
  const PagingResult lru_zero = lru_paging(requests, 0U);
  const PagingResult opt_zero = belady_optimal_paging(requests, 0U);
  REQUIRE_EQ(lru_zero.faults, requests.size());
  REQUIRE_EQ(opt_zero.faults, requests.size());
  verify_generic_trace(requests, lru_zero);
  verify_generic_trace(requests, opt_zero);

  const std::vector<Page> extremes{std::numeric_limits<Page>::min(),
                                   std::numeric_limits<Page>::max(),
                                   std::numeric_limits<Page>::min()};
  const PagingResult extreme_lru = lru_paging(extremes, 1U);
  REQUIRE_EQ(extreme_lru.faults, 3U);
  verify_lru_policy(extremes, extreme_lru);
}

TEST_CASE(lru_trace_replays_exact_recency_policy) {
  const std::vector<Page> requests{1, 2, 3, 1, 4, 5};
  const PagingResult first = lru_paging(requests, 3U);
  const PagingResult second = lru_paging(requests, 3U);
  REQUIRE_EQ(first.faults, 5U);
  REQUIRE(first.events == second.events);
  REQUIRE(first.events[3].hit);
  REQUIRE_EQ(first.events[4].evicted, std::optional<Page>{2});
  REQUIRE_EQ(first.events[5].evicted, std::optional<Page>{3});
  verify_generic_trace(requests, first);
  verify_lru_policy(requests, first);
}

TEST_CASE(belady_offline_tie_is_deterministic) {
  const std::vector<Page> requests{2, 1, 3};
  const PagingResult first = belady_optimal_paging(requests, 2U);
  const PagingResult second = belady_optimal_paging(requests, 2U);
  REQUIRE(first.events == second.events);
  REQUIRE_EQ(first.events[2].evicted, std::optional<Page>{1});
  REQUIRE_EQ(first.faults, exact_offline_faults(requests, 2U));
}

TEST_CASE(paging_exhaustive_small_sequences_match_offline_optimum) {
  for (std::size_t length = 0; length <= 6U; ++length) {
    std::vector<Page> sequence(length, 0);
    enumerate_sequences(0U, sequence, 3U, 1U);
    enumerate_sequences(0U, sequence, 3U, 2U);
  }
}

TEST_CASE(paging_randomized_exact_oracle_and_competitive_boundary) {
  std::mt19937_64 rng(0x0A11CE15ULL);
  std::uniform_int_distribution<int> length_dist(0, 11);
  std::uniform_int_distribution<int> page_dist(-2, 4);
  std::uniform_int_distribution<int> capacity_dist(0, 4);

  for (int trial = 0; trial < 800; ++trial) {
    const std::size_t length = static_cast<std::size_t>(length_dist(rng));
    const std::size_t capacity = static_cast<std::size_t>(capacity_dist(rng));
    std::vector<Page> requests(length);
    for (Page& page : requests) {
      page = static_cast<Page>(page_dist(rng));
    }
    check_against_exact(requests, capacity);
  }
}
