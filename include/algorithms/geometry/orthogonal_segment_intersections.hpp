#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace algorithms::geometry {

struct HorizontalSegment2i {
  Point2i first;
  Point2i second;
  friend bool operator==(const HorizontalSegment2i&, const HorizontalSegment2i&) = default;
};

struct VerticalSegment2i {
  Point2i first;
  Point2i second;
  friend bool operator==(const VerticalSegment2i&, const VerticalSegment2i&) = default;
};

struct OrthogonalSegmentIntersection {
  Point2i point;
  std::size_t horizontal_index;
  std::size_t vertical_index;
  friend bool operator==(const OrthogonalSegmentIntersection&,
                         const OrthogonalSegmentIntersection&) = default;
};

namespace orthogonal_segment_intersections_detail {

constexpr std::int32_t coordinate_limit = 1'000'000'000;

enum class EventKind : unsigned char { horizontal_start, vertical_query, horizontal_end };

struct Event {
  std::int32_t x;
  EventKind kind;
  std::int32_t lower_y;
  std::int32_t upper_y;
  std::size_t index;
};

inline void validate_point(Point2i point) {
  if (point.x < -coordinate_limit || point.x > coordinate_limit ||
      point.y < -coordinate_limit || point.y > coordinate_limit) {
    throw std::out_of_range("orthogonal segment point outside exact coordinate domain");
  }
}

inline std::tuple<std::int32_t, std::int32_t, std::int32_t> normalize_horizontal(
    const HorizontalSegment2i& segment) {
  validate_point(segment.first);
  validate_point(segment.second);
  if (segment.first.y != segment.second.y || segment.first.x == segment.second.x) {
    throw std::invalid_argument("horizontal segment must be non-degenerate and horizontal");
  }
  return {std::min(segment.first.x, segment.second.x),
          std::max(segment.first.x, segment.second.x), segment.first.y};
}

inline std::tuple<std::int32_t, std::int32_t, std::int32_t> normalize_vertical(
    const VerticalSegment2i& segment) {
  validate_point(segment.first);
  validate_point(segment.second);
  if (segment.first.x != segment.second.x || segment.first.y == segment.second.y) {
    throw std::invalid_argument("vertical segment must be non-degenerate and vertical");
  }
  return {segment.first.x, std::min(segment.first.y, segment.second.y),
          std::max(segment.first.y, segment.second.y)};
}

}  // namespace orthogonal_segment_intersections_detail

// Reports every inclusive intersection between the supplied horizontal and vertical
// segment collections. Same-orientation intersections are deliberately outside the
// contract: callers provide the two orientations separately. Input indices are
// preserved in every witness. Output is deterministic and sorted by
// (x, y, horizontal_index, vertical_index).
//
// Sweep invariant: immediately before a vertical query at x, the active map contains
// exactly the horizontal segments whose closed x-interval contains x. Equal-x event
// order is start, query, end, which makes both horizontal endpoints inclusive.
//
// Complexity: O((H + V) log(H + V + 1) + K log(K + 1)) time and
// O(H + V + K) storage. The sweep itself is output-sensitive; the K log(K + 1)
// term comes from the final canonical witness ordering.
[[nodiscard]] inline std::vector<OrthogonalSegmentIntersection>
orthogonal_segment_intersections(std::span<const HorizontalSegment2i> horizontals,
                                 std::span<const VerticalSegment2i> verticals) {
  using namespace orthogonal_segment_intersections_detail;

  if (horizontals.size() >
      (std::numeric_limits<std::size_t>::max() - verticals.size()) / 2U) {
    throw std::length_error("orthogonal segment event count is not representable");
  }

  std::vector<Event> events;
  events.reserve(horizontals.size() * 2U + verticals.size());

  for (std::size_t index = 0; index < horizontals.size(); ++index) {
    const auto [left, right, y] = normalize_horizontal(horizontals[index]);
    events.push_back(Event{left, EventKind::horizontal_start, y, y, index});
    events.push_back(Event{right, EventKind::horizontal_end, y, y, index});
  }
  for (std::size_t index = 0; index < verticals.size(); ++index) {
    const auto [x, lower_y, upper_y] = normalize_vertical(verticals[index]);
    events.push_back(Event{x, EventKind::vertical_query, lower_y, upper_y, index});
  }

  std::sort(events.begin(), events.end(), [](const Event& left, const Event& right) {
    return std::tie(left.x, left.kind, left.lower_y, left.upper_y, left.index) <
           std::tie(right.x, right.kind, right.lower_y, right.upper_y, right.index);
  });

  std::map<std::int32_t, std::set<std::size_t>> active;
  std::vector<OrthogonalSegmentIntersection> result;

  for (const Event& event : events) {
    if (event.kind == EventKind::horizontal_start) {
      const auto [iterator, inserted] = active[event.lower_y].insert(event.index);
      static_cast<void>(iterator);
      if (!inserted) {
        throw std::logic_error("horizontal segment activated more than once");
      }
      continue;
    }
    if (event.kind == EventKind::horizontal_end) {
      const auto active_it = active.find(event.lower_y);
      if (active_it == active.end() || active_it->second.erase(event.index) != 1U) {
        throw std::logic_error("horizontal segment sweep state is inconsistent");
      }
      if (active_it->second.empty()) {
        active.erase(active_it);
      }
      continue;
    }

    auto active_it = active.lower_bound(event.lower_y);
    while (active_it != active.end() && active_it->first <= event.upper_y) {
      for (const std::size_t horizontal_index : active_it->second) {
        result.push_back(OrthogonalSegmentIntersection{
            Point2i{event.x, active_it->first}, horizontal_index, event.index});
      }
      ++active_it;
    }
  }

  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return std::tie(left.point.x, left.point.y, left.horizontal_index, left.vertical_index) <
           std::tie(right.point.x, right.point.y, right.horizontal_index, right.vertical_index);
  });
  return result;
}

}  // namespace algorithms::geometry
