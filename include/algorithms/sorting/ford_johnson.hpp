#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::sorting {

struct FordJohnsonSortResult {
  std::vector<std::int64_t> values;
  std::size_t comparisons{};

  friend bool operator==(const FordJohnsonSortResult&,
                         const FordJohnsonSortResult&) = default;
};

namespace ford_johnson_detail {

struct PairRecord {
  std::size_t low;
  std::size_t high;
};

class IndexSorter {
 public:
  explicit IndexSorter(std::span<const std::int64_t> values) : values_(values) {}

  [[nodiscard]] std::vector<std::size_t> sort(std::vector<std::size_t> ids) {
    if (ids.size() <= 1U) {
      return ids;
    }

    const auto pair_count = ids.size() / 2U;
    std::vector<PairRecord> pairs;
    std::vector<std::size_t> maxima;
    pairs.reserve(pair_count);
    maxima.reserve(pair_count);

    for (std::size_t pair_index = 0; pair_index < pair_count; ++pair_index) {
      const auto first = ids[2U * pair_index];
      const auto second = ids[2U * pair_index + 1U];
      if (less(first, second)) {
        pairs.push_back(PairRecord{first, second});
        maxima.push_back(second);
      } else {
        pairs.push_back(PairRecord{second, first});
        maxima.push_back(first);
      }
    }

    const bool has_stray = ids.size() % 2U != 0U;
    const auto stray = has_stray ? ids.back() : std::size_t{0};
    auto sorted_maxima = sort(std::move(maxima));

    std::vector<std::size_t> chain;
    chain.reserve(ids.size());
    chain.push_back(low_for_high(pairs, sorted_maxima.front()));
    chain.insert(chain.end(), sorted_maxima.begin(), sorted_maxima.end());

    struct Pending {
      std::size_t value;
      std::size_t partner;
      bool has_partner;
    };

    const auto total_pending_index = pair_count + (has_stray ? 1U : 0U);
    std::vector<Pending> pending(total_pending_index + 1U,
                                 Pending{0U, 0U, false});
    for (std::size_t index = 2U; index <= pair_count; ++index) {
      const auto high = sorted_maxima[index - 1U];
      pending[index] = Pending{low_for_high(pairs, high), high, true};
    }
    if (has_stray) {
      pending[pair_count + 1U] = Pending{stray, 0U, false};
    }

    std::size_t previous_boundary = 1U;
    std::size_t previous_jacobsthal = 1U;
    std::size_t jacobsthal = 3U;
    while (previous_boundary < total_pending_index) {
      const auto boundary = std::min(jacobsthal, total_pending_index);
      for (std::size_t index = boundary; index > previous_boundary; --index) {
        const auto item = pending[index];
        const auto search_end = item.has_partner
                                    ? partner_position(chain, item.partner)
                                    : chain.size();
        const auto insertion = lower_bound_position(chain, item.value, search_end);
        chain.insert(chain.begin() + static_cast<std::ptrdiff_t>(insertion),
                     item.value);
      }
      previous_boundary = boundary;
      if (previous_boundary == total_pending_index) {
        break;
      }

      const auto remaining = total_pending_index - jacobsthal;
      std::size_t next = total_pending_index;
      if (previous_jacobsthal < (remaining + 1U) / 2U) {
        next = jacobsthal + 2U * previous_jacobsthal;
      }
      previous_jacobsthal = jacobsthal;
      jacobsthal = next;
    }

    return chain;
  }

  [[nodiscard]] std::size_t comparisons() const noexcept { return comparisons_; }

 private:
  [[nodiscard]] bool less(std::size_t lhs, std::size_t rhs) {
    ++comparisons_;
    return values_[lhs] < values_[rhs];
  }

  [[nodiscard]] static std::size_t low_for_high(
      const std::vector<PairRecord>& pairs, std::size_t high) {
    const auto iterator = std::find_if(
        pairs.begin(), pairs.end(),
        [high](const PairRecord& pair) { return pair.high == high; });
    if (iterator == pairs.end()) {
      throw std::logic_error("Ford-Johnson partner invariant violated");
    }
    return iterator->low;
  }

  [[nodiscard]] static std::size_t partner_position(
      const std::vector<std::size_t>& chain, std::size_t partner) {
    const auto iterator = std::find(chain.begin(), chain.end(), partner);
    if (iterator == chain.end()) {
      throw std::logic_error("Ford-Johnson partner missing from main chain");
    }
    return static_cast<std::size_t>(iterator - chain.begin());
  }

  [[nodiscard]] std::size_t lower_bound_position(
      const std::vector<std::size_t>& chain, std::size_t value,
      std::size_t end) {
    std::size_t begin = 0U;
    while (begin < end) {
      const auto middle = begin + (end - begin) / 2U;
      if (less(chain[middle], value)) {
        begin = middle + 1U;
      } else {
        end = middle;
      }
    }
    return begin;
  }

  std::span<const std::int64_t> values_;
  std::size_t comparisons_{};
};

}  // namespace ford_johnson_detail

// Sorts a copy of `input` using Ford-Johnson merge-insertion sorting and
// returns the exact number of value comparisons performed by this implementation.
// The implementation studies comparison count rather than data-movement cost:
// vector insertion and partner lookup make this direct baseline O(n^2) in
// ordinary bookkeeping even though its comparison count follows the classical
// merge-insertion bound.
[[nodiscard]] inline FordJohnsonSortResult ford_johnson_sort(
    std::span<const std::int64_t> input) {
  ford_johnson_detail::IndexSorter sorter(input);
  std::vector<std::size_t> ids(input.size());
  for (std::size_t index = 0; index < ids.size(); ++index) {
    ids[index] = index;
  }
  const auto sorted_ids = sorter.sort(std::move(ids));

  std::vector<std::int64_t> values;
  values.reserve(input.size());
  for (const auto index : sorted_ids) {
    values.push_back(input[index]);
  }
  return FordJohnsonSortResult{std::move(values), sorter.comparisons()};
}

}  // namespace algorithms::sorting
