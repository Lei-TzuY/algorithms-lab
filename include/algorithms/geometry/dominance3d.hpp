#pragma once

#include "algorithms/data_structures/fenwick_tree.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace algorithms::geometry {

struct Point3i {
  std::int64_t x;
  std::int64_t y;
  std::int64_t z;

  friend bool operator==(const Point3i&, const Point3i&) = default;
};

// For every input point p_i, return the number of input points p_j satisfying
// p_j.x <= p_i.x, p_j.y <= p_i.y, and p_j.z <= p_i.z. The count includes the
// point itself and every duplicate copy of the same coordinate triple.
//
// Exact duplicates are collapsed into weighted groups. CDQ divide-and-conquer
// follows lexicographic (x,y,z) order; each merge streams left-half groups by y
// into the sealed Phase-4 Fenwick tree over coordinate-compressed z.
//
// Time: O(n log^2 n). Auxiliary/result storage: O(n).
[[nodiscard]] inline std::vector<std::uint64_t> dominance_counts_3d(
    std::span<const Point3i> points) {
  if (points.size() >
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
    throw std::length_error("3D dominance input exceeds Fenwick count domain");
  }
  if (points.empty()) return {};

  struct IndexedPoint {
    Point3i point;
    std::size_t original_index;
  };
  std::vector<IndexedPoint> sorted;
  sorted.reserve(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    sorted.push_back(IndexedPoint{points[i], i});
  }
  std::sort(sorted.begin(), sorted.end(), [](const IndexedPoint& a,
                                              const IndexedPoint& b) {
    return std::tie(a.point.x, a.point.y, a.point.z) <
           std::tie(b.point.x, b.point.y, b.point.z);
  });

  struct Group {
    Point3i point;
    std::int64_t weight;
    std::size_t z_rank;
    std::uint64_t dominated;
  };
  std::vector<Group> groups;
  groups.reserve(sorted.size());
  std::vector<std::size_t> original_group(points.size());

  for (std::size_t i = 0; i < sorted.size();) {
    const Point3i point = sorted[i].point;
    std::size_t j = i + 1U;
    while (j < sorted.size() && sorted[j].point == point) ++j;
    const std::size_t multiplicity = j - i;
    if (multiplicity >
        static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
      throw std::length_error(
          "3D dominance duplicate group exceeds Fenwick count domain");
    }
    const std::size_t group_index = groups.size();
    groups.push_back(Group{point, static_cast<std::int64_t>(multiplicity), 0U,
                           static_cast<std::uint64_t>(multiplicity)});
    for (std::size_t k = i; k < j; ++k) {
      original_group[sorted[k].original_index] = group_index;
    }
    i = j;
  }

  std::vector<std::int64_t> z_values;
  z_values.reserve(groups.size());
  for (const Group& group : groups) z_values.push_back(group.point.z);
  std::sort(z_values.begin(), z_values.end());
  z_values.erase(std::unique(z_values.begin(), z_values.end()), z_values.end());
  for (Group& group : groups) {
    group.z_rank = static_cast<std::size_t>(
        std::lower_bound(z_values.begin(), z_values.end(), group.point.z) -
        z_values.begin());
  }

  std::vector<std::size_t> order(groups.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::vector<std::size_t> scratch(order.size());
  algorithms::data_structures::FenwickTree fenwick(z_values.size());

  const auto cdq = [&](auto&& self, std::size_t begin,
                       std::size_t end) -> void {
    if (end - begin <= 1U) return;
    const std::size_t middle = begin + (end - begin) / 2U;
    self(self, begin, middle);
    self(self, middle, end);

    std::size_t left = begin;
    for (std::size_t right = middle; right < end; ++right) {
      const Group& target = groups[order[right]];
      while (left < middle &&
             groups[order[left]].point.y <= target.point.y) {
        const Group& source = groups[order[left]];
        fenwick.add(source.z_rank, source.weight);
        ++left;
      }
      const std::int64_t contribution =
          fenwick.prefix_sum(target.z_rank + 1U);
      if (contribution < 0) {
        throw std::logic_error(
            "3D dominance Fenwick contribution became negative");
      }
      groups[order[right]].dominated +=
          static_cast<std::uint64_t>(contribution);
    }
    for (std::size_t i = begin; i < left; ++i) {
      const Group& source = groups[order[i]];
      fenwick.add(source.z_rank, -source.weight);
    }

    std::size_t i = begin;
    std::size_t j = middle;
    std::size_t out = begin;
    const auto left_precedes = [&](std::size_t lhs, std::size_t rhs) {
      const Group& a = groups[lhs];
      const Group& b = groups[rhs];
      return std::tie(a.point.y, a.point.z, a.point.x) <=
             std::tie(b.point.y, b.point.z, b.point.x);
    };
    while (i < middle && j < end) {
      if (left_precedes(order[i], order[j])) {
        scratch[out++] = order[i++];
      } else {
        scratch[out++] = order[j++];
      }
    }
    while (i < middle) scratch[out++] = order[i++];
    while (j < end) scratch[out++] = order[j++];
    for (std::size_t k = begin; k < end; ++k) order[k] = scratch[k];
  };
  cdq(cdq, 0U, order.size());

  std::vector<std::uint64_t> result(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    result[i] = groups[original_group[i]].dominated;
  }
  return result;
}

}  // namespace algorithms::geometry
