#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace algorithms::geometry {

struct PlanarDiameterResult {
  Point2i first;
  Point2i second;
  std::int64_t squared_distance;

  friend bool operator==(const PlanarDiameterResult&,
                         const PlanarDiameterResult&) = default;
};

// Returns an exact farthest pair under squared Euclidean distance in the
// repository's bounded Point2i domain. Endpoints are canonicalized
// lexicographically; equal-diameter pairs are broken by that pair key.
// Fewer than two input points returns std::nullopt.
[[nodiscard]] std::optional<PlanarDiameterResult> planar_diameter(
    std::span<const Point2i> points);

}  // namespace algorithms::geometry
