#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::online {

using Page = std::int64_t;

struct PagingEvent {
  Page page{};
  bool hit{false};
  std::optional<Page> evicted;

  friend bool operator==(const PagingEvent&, const PagingEvent&) = default;
};

struct PagingResult {
  std::size_t capacity{0};
  std::size_t faults{0};
  std::vector<PagingEvent> events;
};

// Online LRU: decisions use only the request prefix seen so far.
[[nodiscard]] PagingResult lru_paging(std::span<const Page> requests,
                                      std::size_t capacity);

// Offline comparator: Belady's furthest-in-future optimal paging algorithm.
// This routine intentionally uses future requests and is not an online policy.
[[nodiscard]] PagingResult belady_optimal_paging(
    std::span<const Page> requests, std::size_t capacity);

}  // namespace algorithms::online
