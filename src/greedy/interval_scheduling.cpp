#include "algorithms/greedy/interval_scheduling.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::greedy {

std::vector<std::size_t> select_maximum_compatible_intervals(
    const std::vector<Interval>& intervals) {
  std::vector<std::size_t> order;
  order.reserve(intervals.size());

  for (std::size_t index = 0; index < intervals.size(); ++index) {
    if (intervals[index].start > intervals[index].finish) {
      throw std::invalid_argument("interval start must not exceed finish");
    }
    order.push_back(index);
  }

  std::sort(order.begin(), order.end(),
            [&intervals](std::size_t left, std::size_t right) {
              const Interval& a = intervals[left];
              const Interval& b = intervals[right];
              if (a.finish != b.finish) {
                return a.finish < b.finish;
              }
              if (a.start != b.start) {
                return a.start < b.start;
              }
              return left < right;
            });

  std::vector<std::size_t> selected;
  std::optional<std::int64_t> last_finish;
  for (const std::size_t index : order) {
    const Interval& interval = intervals[index];
    if (!last_finish.has_value() || interval.start >= *last_finish) {
      selected.push_back(index);
      last_finish = interval.finish;
    }
  }
  return selected;
}

}  // namespace algorithms::greedy
