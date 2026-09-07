#include "algorithms/online/paging.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <utility>

namespace algorithms::online {
namespace {

PagingResult zero_capacity_result(std::span<const Page> requests) {
  PagingResult result;
  result.capacity = 0;
  result.faults = requests.size();
  result.events.reserve(requests.size());
  for (const Page page : requests) {
    result.events.push_back(PagingEvent{page, false, std::nullopt});
  }
  return result;
}

}  // namespace

PagingResult lru_paging(std::span<const Page> requests,
                        std::size_t capacity) {
  if (capacity == 0U) {
    return zero_capacity_result(requests);
  }

  PagingResult result;
  result.capacity = capacity;
  result.events.reserve(requests.size());

  std::list<Page> recency;  // front = MRU, back = LRU
  std::map<Page, std::list<Page>::iterator> positions;

  for (const Page page : requests) {
    const auto found = positions.find(page);
    if (found != positions.end()) {
      recency.splice(recency.begin(), recency, found->second);
      found->second = recency.begin();
      result.events.push_back(PagingEvent{page, true, std::nullopt});
      continue;
    }

    ++result.faults;
    std::optional<Page> evicted;
    if (recency.size() == capacity) {
      const Page victim = recency.back();
      recency.pop_back();
      positions.erase(victim);
      evicted = victim;
    }

    recency.push_front(page);
    positions.emplace(page, recency.begin());
    result.events.push_back(PagingEvent{page, false, evicted});
  }

  return result;
}

PagingResult belady_optimal_paging(std::span<const Page> requests,
                                   std::size_t capacity) {
  if (capacity == 0U) {
    return zero_capacity_result(requests);
  }

  PagingResult result;
  result.capacity = capacity;
  result.events.reserve(requests.size());

  std::map<Page, std::deque<std::size_t>> future_positions;
  for (std::size_t index = 0; index < requests.size(); ++index) {
    future_positions[requests[index]].push_back(index);
  }

  std::vector<Page> cache;
  cache.reserve(std::min(capacity, requests.size()));

  for (std::size_t index = 0; index < requests.size(); ++index) {
    const Page page = requests[index];
    auto& positions = future_positions[page];
    positions.pop_front();

    const auto hit_it = std::find(cache.begin(), cache.end(), page);
    if (hit_it != cache.end()) {
      result.events.push_back(PagingEvent{page, true, std::nullopt});
      continue;
    }

    ++result.faults;
    std::optional<Page> evicted;
    if (cache.size() == capacity) {
      std::size_t victim_index = 0;
      std::size_t farthest_next = 0;
      bool victim_never_used_again = false;

      for (std::size_t candidate = 0; candidate < cache.size(); ++candidate) {
        const Page cached_page = cache[candidate];
        const auto& cached_future = future_positions[cached_page];
        const bool never_used_again = cached_future.empty();
        const std::size_t next_use = never_used_again
                                         ? std::numeric_limits<std::size_t>::max()
                                         : cached_future.front();

        bool choose = false;
        if (candidate == 0U) {
          choose = true;
        } else if (never_used_again != victim_never_used_again) {
          choose = never_used_again;
        } else if (next_use > farthest_next) {
          choose = true;
        } else if (next_use == farthest_next &&
                   cached_page < cache[victim_index]) {
          // The only practical next-use tie is "never again"; use the
          // numerically smaller page as a deterministic witness tie-break.
          choose = true;
        }

        if (choose) {
          victim_index = candidate;
          farthest_next = next_use;
          victim_never_used_again = never_used_again;
        }
      }

      evicted = cache[victim_index];
      cache[victim_index] = page;
    } else {
      cache.push_back(page);
    }

    result.events.push_back(PagingEvent{page, false, evicted});
  }

  return result;
}

}  // namespace algorithms::online
