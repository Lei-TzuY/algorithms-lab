#pragma once

#include <cstdint>
#include <span>

namespace algorithms::geometry {

struct AxisAlignedRectangle {
  std::int32_t min_x;
  std::int32_t min_y;
  std::int32_t max_x;
  std::int32_t max_y;

  friend bool operator==(const AxisAlignedRectangle&,
                         const AxisAlignedRectangle&) = default;
};

// Returns the exact area covered by at least one rectangle. Coordinates must
// lie in [-1e9, 1e9], min coordinates must not exceed max coordinates, and
// zero-width/zero-height rectangles are accepted as zero-area inputs.
[[nodiscard]] std::int64_t rectangle_union_area(
    std::span<const AxisAlignedRectangle> rectangles);

}  // namespace algorithms::geometry
