#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::offline {

struct RangeQuery {
  std::size_t begin{};
  std::size_t end{};

  friend bool operator==(const RangeQuery&, const RangeQuery&) = default;
};

// Answer static distinct-value counts for half-open ranges [begin, end) using
// Mo's offline ordering. Queries are returned in their original input order.
//
// Preconditions:
// - begin <= end
// - end <= values.size()
//
// Complexity for N values and Q queries:
// - O(N log N + Q log Q) preprocessing/order construction
// - O((N + Q) sqrt(N)) add/remove work with the chosen sqrt-sized blocks
// - O(N + Q) auxiliary storage after coordinate compression
[[nodiscard]] std::vector<std::size_t> mo_distinct_counts(
    std::span<const std::int64_t> values,
    std::span<const RangeQuery> queries);

}  // namespace algorithms::offline
