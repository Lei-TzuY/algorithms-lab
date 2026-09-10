#include "algorithms/geometry/rectangle_union.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::geometry {
namespace {

constexpr std::int32_t kCoordinateLimit = 1'000'000'000;

struct SweepEvent {
  std::int32_t x;
  std::int32_t min_y;
  std::int32_t max_y;
  bool entering;
};

void validate_coordinate(std::int32_t value) {
  if (value < -kCoordinateLimit || value > kCoordinateLimit) {
    throw std::out_of_range("rectangle coordinate exceeds exact domain");
  }
}

void validate_rectangle(const AxisAlignedRectangle& rectangle) {
  validate_coordinate(rectangle.min_x);
  validate_coordinate(rectangle.min_y);
  validate_coordinate(rectangle.max_x);
  validate_coordinate(rectangle.max_y);
  if (rectangle.min_x > rectangle.max_x ||
      rectangle.min_y > rectangle.max_y) {
    throw std::invalid_argument("rectangle bounds are reversed");
  }
}

[[nodiscard]] std::size_t tree_storage_size(std::size_t interval_count) {
  if (interval_count > std::numeric_limits<std::size_t>::max() / 4U) {
    throw std::length_error("rectangle sweep tree is too large");
  }
  return interval_count * 4U;
}

class CoveredLengthTree {
 public:
  explicit CoveredLengthTree(const std::vector<std::int32_t>& coordinates)
      : coordinates_(coordinates),
        interval_count_(coordinates.size() - 1U),
        cover_count_(tree_storage_size(interval_count_), 0U),
        covered_length_(tree_storage_size(interval_count_), 0) {}

  void update(std::size_t query_begin, std::size_t query_end, bool entering) {
    update_node(1U, 0U, interval_count_, query_begin, query_end, entering);
  }

  [[nodiscard]] std::int64_t covered_length() const noexcept {
    return covered_length_[1U];
  }

 private:
  void pull(std::size_t node, std::size_t begin, std::size_t end) {
    if (cover_count_[node] != 0U) {
      covered_length_[node] =
          static_cast<std::int64_t>(coordinates_[end]) -
          static_cast<std::int64_t>(coordinates_[begin]);
      return;
    }
    if (end - begin == 1U) {
      covered_length_[node] = 0;
      return;
    }
    covered_length_[node] =
        covered_length_[node * 2U] + covered_length_[node * 2U + 1U];
  }

  void update_node(std::size_t node, std::size_t begin, std::size_t end,
                   std::size_t query_begin, std::size_t query_end,
                   bool entering) {
    if (query_begin <= begin && end <= query_end) {
      if (entering) {
        ++cover_count_[node];
      } else {
        if (cover_count_[node] == 0U) {
          throw std::logic_error("rectangle sweep coverage underflow");
        }
        --cover_count_[node];
      }
      pull(node, begin, end);
      return;
    }

    const std::size_t middle = begin + (end - begin) / 2U;
    if (query_begin < middle) {
      update_node(node * 2U, begin, middle, query_begin, query_end, entering);
    }
    if (middle < query_end) {
      update_node(node * 2U + 1U, middle, end, query_begin, query_end,
                  entering);
    }
    pull(node, begin, end);
  }

  const std::vector<std::int32_t>& coordinates_;
  std::size_t interval_count_;
  std::vector<std::size_t> cover_count_;
  std::vector<std::int64_t> covered_length_;
};

}  // namespace

std::int64_t rectangle_union_area(
    std::span<const AxisAlignedRectangle> rectangles) {
  if (rectangles.size() > std::numeric_limits<std::size_t>::max() / 2U) {
    throw std::length_error("too many rectangles for sweep events");
  }
  const std::size_t endpoint_capacity = rectangles.size() * 2U;

  std::vector<SweepEvent> events;
  std::vector<std::int32_t> y_coordinates;
  events.reserve(endpoint_capacity);
  y_coordinates.reserve(endpoint_capacity);

  for (const auto& rectangle : rectangles) {
    validate_rectangle(rectangle);
    if (rectangle.min_x == rectangle.max_x ||
        rectangle.min_y == rectangle.max_y) {
      continue;
    }
    events.push_back(
        SweepEvent{rectangle.min_x, rectangle.min_y, rectangle.max_y, true});
    events.push_back(
        SweepEvent{rectangle.max_x, rectangle.min_y, rectangle.max_y, false});
    y_coordinates.push_back(rectangle.min_y);
    y_coordinates.push_back(rectangle.max_y);
  }

  if (events.empty()) {
    return 0;
  }

  std::sort(y_coordinates.begin(), y_coordinates.end());
  y_coordinates.erase(
      std::unique(y_coordinates.begin(), y_coordinates.end()),
      y_coordinates.end());

  std::sort(events.begin(), events.end(),
            [](const SweepEvent& left, const SweepEvent& right) {
              if (left.x != right.x) {
                return left.x < right.x;
              }
              if (left.min_y != right.min_y) {
                return left.min_y < right.min_y;
              }
              if (left.max_y != right.max_y) {
                return left.max_y < right.max_y;
              }
              return static_cast<int>(left.entering) <
                     static_cast<int>(right.entering);
            });

  CoveredLengthTree tree(y_coordinates);
  std::int64_t area = 0;
  std::int32_t previous_x = events.front().x;
  std::size_t event_index = 0U;

  while (event_index < events.size()) {
    const std::int32_t current_x = events[event_index].x;
    const std::int64_t delta_x = static_cast<std::int64_t>(current_x) -
                                 static_cast<std::int64_t>(previous_x);
    area += delta_x * tree.covered_length();

    while (event_index < events.size() && events[event_index].x == current_x) {
      const auto& event = events[event_index];
      const auto lower = std::lower_bound(y_coordinates.begin(),
                                          y_coordinates.end(), event.min_y);
      const auto upper = std::lower_bound(y_coordinates.begin(),
                                          y_coordinates.end(), event.max_y);
      const auto begin_index =
          static_cast<std::size_t>(lower - y_coordinates.begin());
      const auto end_index =
          static_cast<std::size_t>(upper - y_coordinates.begin());
      tree.update(begin_index, end_index, event.entering);
      ++event_index;
    }
    previous_x = current_x;
  }

  return area;
}

}  // namespace algorithms::geometry
