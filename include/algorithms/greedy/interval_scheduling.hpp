#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::greedy {

struct Interval {
  std::int64_t start;
  std::int64_t finish;

  friend bool operator==(const Interval&, const Interval&) = default;
};

// Intervals use half-open semantics [start, finish), so touching endpoints are
// compatible. Every interval must satisfy start <= finish.
//
// Greedy invariant: after each selection, the chosen schedule has the earliest
// possible finishing time among all schedules with the same number of chosen
// intervals from the processed prefix. Therefore it leaves at least as much
// room for future intervals as any competing schedule.
//
// Returns input indices in the order selected.
// Time: O(n log n), space: O(n).
[[nodiscard]] std::vector<std::size_t> select_maximum_compatible_intervals(
    const std::vector<Interval>& intervals);

}  // namespace algorithms::greedy
