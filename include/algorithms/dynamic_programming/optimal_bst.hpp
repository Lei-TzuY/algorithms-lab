#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::dynamic_programming {

struct UnsignedCost128 {
  std::uint64_t high = 0;
  std::uint64_t low = 0;

  friend bool operator==(const UnsignedCost128&, const UnsignedCost128&) = default;

  [[nodiscard]] bool fits_uint64() const noexcept { return high == 0; }

  [[nodiscard]] std::uint64_t to_uint64() const {
    if (!fits_uint64()) {
      throw std::overflow_error("optimal BST cost does not fit uint64_t");
    }
    return low;
  }
};

struct OptimalBstResult {
  static constexpr std::size_t no_root = std::numeric_limits<std::size_t>::max();

  UnsignedCost128 weighted_search_cost;
  std::optional<std::size_t> root;
  std::vector<std::optional<std::size_t>> parent;
  std::vector<std::size_t> key_depths;
  std::vector<std::size_t> gap_depths;

  // Diagnostic table. Entry [i * root_table_stride + j] is the chosen root
  // for the non-empty half-open key interval [i,j). Empty/unused entries are
  // no_root. It is exposed so the Knuth monotonicity obligation is replayable.
  std::size_t root_table_stride = 0;
  std::vector<std::size_t> interval_roots;
  std::size_t candidate_evaluations = 0;
};

namespace detail {

struct WideUnsigned {
  std::uint64_t high = 0;
  std::uint64_t low = 0;
};

[[nodiscard]] inline bool less(const WideUnsigned& lhs,
                               const WideUnsigned& rhs) noexcept {
  return lhs.high < rhs.high ||
         (lhs.high == rhs.high && lhs.low < rhs.low);
}

[[nodiscard]] inline WideUnsigned add(const WideUnsigned& lhs,
                                      const WideUnsigned& rhs) {
  const std::uint64_t low = lhs.low + rhs.low;
  const std::uint64_t carry = low < lhs.low ? 1U : 0U;

  const std::uint64_t high_without_carry = lhs.high + rhs.high;
  if (high_without_carry < lhs.high) {
    throw std::overflow_error("optimal BST internal 128-bit cost overflow");
  }
  if (carry != 0U &&
      high_without_carry == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("optimal BST internal 128-bit cost overflow");
  }
  return WideUnsigned{high_without_carry + carry, low};
}

[[nodiscard]] inline WideUnsigned subtract(const WideUnsigned& lhs,
                                           const WideUnsigned& rhs) {
  if (less(lhs, rhs)) {
    throw std::logic_error("optimal BST internal prefix subtraction underflow");
  }
  const std::uint64_t borrow = lhs.low < rhs.low ? 1U : 0U;
  return WideUnsigned{lhs.high - rhs.high - borrow, lhs.low - rhs.low};
}

[[nodiscard]] inline WideUnsigned from_u64(std::uint64_t value) noexcept {
  return WideUnsigned{0, value};
}

[[nodiscard]] inline UnsignedCost128 public_cost(const WideUnsigned& value) noexcept {
  return UnsignedCost128{value.high, value.low};
}

}  // namespace detail

// Exact optimal binary-search-tree dynamic program with Knuth root narrowing.
// Keys are the implicit sorted indices [0,n). successful[i] is the weight of
// finding key i; unsuccessful[g] is the weight of falling into gap g, with
// unsuccessful.size() == n + 1.
//
// Cost convention: a key or gap at depth d contributes weight * (d + 1).
// Therefore the empty tree has cost unsuccessful[0].
[[nodiscard]] inline OptimalBstResult optimal_binary_search_tree_knuth(
    std::span<const std::uint64_t> successful,
    std::span<const std::uint64_t> unsuccessful) {
  const std::size_t n = successful.size();
  if (unsuccessful.size() != n + 1U) {
    throw std::invalid_argument(
        "optimal BST requires exactly n+1 unsuccessful-search weights");
  }

  if (n == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("optimal BST table dimension is not representable");
  }
  const std::size_t stride = n + 1U;
  if (stride != 0U && stride > std::numeric_limits<std::size_t>::max() / stride) {
    throw std::length_error("optimal BST quadratic table is too large");
  }
  const std::size_t cells = stride * stride;
  constexpr std::size_t bytes_per_cell =
      sizeof(detail::WideUnsigned) + sizeof(std::size_t);
  if (cells > std::numeric_limits<std::size_t>::max() / bytes_per_cell) {
    throw std::length_error("optimal BST quadratic state is too large");
  }

  std::vector<detail::WideUnsigned> successful_prefix(n + 1U);
  for (std::size_t i = 0; i < n; ++i) {
    successful_prefix[i + 1U] = detail::add(
        successful_prefix[i], detail::from_u64(successful[i]));
  }

  std::vector<detail::WideUnsigned> unsuccessful_prefix(n + 2U);
  for (std::size_t i = 0; i <= n; ++i) {
    unsuccessful_prefix[i + 1U] = detail::add(
        unsuccessful_prefix[i], detail::from_u64(unsuccessful[i]));
  }

  const auto interval_weight = [&](std::size_t begin,
                                   std::size_t end) -> detail::WideUnsigned {
    const detail::WideUnsigned successful_sum = detail::subtract(
        successful_prefix[end], successful_prefix[begin]);
    const detail::WideUnsigned unsuccessful_sum = detail::subtract(
        unsuccessful_prefix[end + 1U], unsuccessful_prefix[begin]);
    return detail::add(successful_sum, unsuccessful_sum);
  };

  std::vector<detail::WideUnsigned> costs(cells);
  std::vector<std::size_t> roots(cells, OptimalBstResult::no_root);
  const auto table_index = [stride](std::size_t begin, std::size_t end) {
    return begin * stride + end;
  };

  for (std::size_t i = 0; i <= n; ++i) {
    costs[table_index(i, i)] = detail::from_u64(unsuccessful[i]);
  }

  std::size_t candidate_evaluations = 0;
  for (std::size_t length = 1; length <= n; ++length) {
    for (std::size_t begin = 0; begin + length <= n; ++begin) {
      const std::size_t end = begin + length;
      std::size_t low_root = begin;
      std::size_t high_root = end - 1U;
      if (length > 1U) {
        low_root = roots[table_index(begin, end - 1U)];
        high_root = roots[table_index(begin + 1U, end)];
        if (low_root == OptimalBstResult::no_root ||
            high_root == OptimalBstResult::no_root || low_root > high_root ||
            low_root < begin || high_root >= end) {
          throw std::logic_error("optimal BST Knuth root bounds are invalid");
        }
      }

      const std::size_t candidate_count = high_root - low_root + 1U;
      if (candidate_evaluations >
          std::numeric_limits<std::size_t>::max() - candidate_count) {
        throw std::overflow_error("optimal BST candidate counter overflow");
      }
      candidate_evaluations += candidate_count;

      const detail::WideUnsigned weight = interval_weight(begin, end);
      bool have_best = false;
      detail::WideUnsigned best{};
      std::size_t best_root = OptimalBstResult::no_root;
      for (std::size_t root = low_root; root <= high_root; ++root) {
        const detail::WideUnsigned children = detail::add(
            costs[table_index(begin, root)],
            costs[table_index(root + 1U, end)]);
        const detail::WideUnsigned candidate = detail::add(children, weight);
        if (!have_best || detail::less(candidate, best)) {
          have_best = true;
          best = candidate;
          best_root = root;
        }
      }
      if (!have_best) {
        throw std::logic_error("optimal BST interval has no root candidate");
      }
      costs[table_index(begin, end)] = best;
      roots[table_index(begin, end)] = best_root;
    }
  }

  OptimalBstResult result;
  result.weighted_search_cost = detail::public_cost(costs[table_index(0, n)]);
  result.parent.assign(n, std::nullopt);
  result.key_depths.assign(n, 0U);
  result.gap_depths.assign(n + 1U, 0U);
  result.root_table_stride = stride;
  result.interval_roots = roots;
  result.candidate_evaluations = candidate_evaluations;

  struct PendingInterval {
    std::size_t begin;
    std::size_t end;
    std::optional<std::size_t> parent;
    std::size_t depth;
  };

  std::vector<PendingInterval> pending;
  pending.push_back(PendingInterval{0, n, std::nullopt, 0});
  while (!pending.empty()) {
    const PendingInterval current = pending.back();
    pending.pop_back();
    if (current.begin == current.end) {
      result.gap_depths[current.begin] = current.depth;
      continue;
    }

    const std::size_t root = roots[table_index(current.begin, current.end)];
    if (root == OptimalBstResult::no_root) {
      throw std::logic_error("optimal BST reconstruction root is missing");
    }
    result.parent[root] = current.parent;
    result.key_depths[root] = current.depth;
    if (!current.parent.has_value()) {
      result.root = root;
    }

    if (current.depth == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("optimal BST reconstruction depth overflow");
    }
    const std::size_t child_depth = current.depth + 1U;
    pending.push_back(
        PendingInterval{root + 1U, current.end, root, child_depth});
    pending.push_back(
        PendingInterval{current.begin, root, root, child_depth});
  }

  return result;
}

}  // namespace algorithms::dynamic_programming
