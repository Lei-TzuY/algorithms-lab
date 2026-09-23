#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::dynamic_programming {

// Returns a deterministic maximum-cardinality noncrossing matching on ordered
// vertices 0..vertex_count-1.
//
// allowed_edges describes an undirected compatibility relation. Duplicate edge
// declarations are accepted. Self-loops and out-of-range endpoints are
// rejected.
//
// Two selected edges (a,b) and (c,d), normalized so a<b and c<d, cross when
// a<c<b<d or c<a<d<b. Nested and disjoint edges are allowed.
//
// Among equal-cardinality solutions the implementation is deterministic but
// does not claim a global lexicographic optimum: leaving the current leftmost
// vertex unmatched wins exact ties; otherwise the smallest partner that first
// improves the optimum is retained.
[[nodiscard]] inline std::vector<std::pair<std::size_t, std::size_t>>
maximum_noncrossing_matching(
    const std::size_t vertex_count,
    const std::span<const std::pair<std::size_t, std::size_t>>
        allowed_edges) {
  if (vertex_count == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error(
        "noncrossing matching vertex count is too large");
  }

  std::vector<std::vector<unsigned char>> allowed(
      vertex_count,
      std::vector<unsigned char>(vertex_count, 0U));

  for (const auto& [raw_first, raw_second] : allowed_edges) {
    if (raw_first >= vertex_count || raw_second >= vertex_count) {
      throw std::out_of_range(
          "noncrossing matching edge endpoint out of range");
    }
    if (raw_first == raw_second) {
      throw std::invalid_argument(
          "noncrossing matching does not accept self-loops");
    }

    const std::size_t first = std::min(raw_first, raw_second);
    const std::size_t second = std::max(raw_first, raw_second);
    allowed[first][second] = 1U;
  }

  if (vertex_count <= 1U || allowed_edges.empty()) {
    return {};
  }

  const std::size_t npos = std::numeric_limits<std::size_t>::max();
  std::vector<std::vector<std::size_t>> optimum(
      vertex_count + 1U,
      std::vector<std::size_t>(vertex_count + 1U, 0U));
  std::vector<std::vector<std::size_t>> partner(
      vertex_count + 1U,
      std::vector<std::size_t>(vertex_count + 1U, npos));

  // optimum[left][right] covers the half-open ordered interval [left,right).
  // In every noncrossing matching, the leftmost vertex is either unmatched or
  // paired with some k. In the paired case, noncrossing edges split into the
  // independent inside interval [left+1,k) and outside interval [k+1,right).
  for (std::size_t length = 2U; length <= vertex_count; ++length) {
    for (std::size_t left = 0U;
         left + length <= vertex_count; ++left) {
      const std::size_t right = left + length;

      std::size_t best = optimum[left + 1U][right];
      std::size_t best_partner = npos;

      for (std::size_t candidate = left + 1U;
           candidate < right; ++candidate) {
        if (allowed[left][candidate] == 0U) {
          continue;
        }

        const std::size_t value =
            1U + optimum[left + 1U][candidate] +
            optimum[candidate + 1U][right];
        if (value > best) {
          best = value;
          best_partner = candidate;
        }
      }

      optimum[left][right] = best;
      partner[left][right] = best_partner;
    }
  }

  std::vector<std::pair<std::size_t, std::size_t>> result;
  result.reserve(optimum[0U][vertex_count]);

  std::vector<std::pair<std::size_t, std::size_t>> pending;
  pending.emplace_back(0U, vertex_count);

  while (!pending.empty()) {
    const auto [left, right] = pending.back();
    pending.pop_back();

    if (right <= left + 1U) {
      continue;
    }

    const std::size_t matched = partner[left][right];
    if (matched == npos) {
      pending.emplace_back(left + 1U, right);
      continue;
    }

    result.emplace_back(left, matched);

    // Push the outside interval first so the inside interval is processed first.
    if (matched + 1U < right) {
      pending.emplace_back(matched + 1U, right);
    }
    if (left + 1U < matched) {
      pending.emplace_back(left + 1U, matched);
    }
  }

  std::sort(result.begin(), result.end());
  if (result.size() != optimum[0U][vertex_count]) {
    throw std::logic_error(
        "noncrossing matching reconstruction invariant violated");
  }

  return result;
}

}  // namespace algorithms::dynamic_programming
