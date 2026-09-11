#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::geometry {

struct KdNearestResult {
  Point2i point;
  std::int64_t squared_distance;
  std::size_t visited_nodes;
  std::size_t pruned_subtrees;

  friend bool operator==(const KdNearestResult&, const KdNearestResult&) = default;
};

class KdTree2D {
 public:
  explicit KdTree2D(std::span<const Point2i> points);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t height() const noexcept;

  // Exact nearest-neighbor query over the accepted geometry coordinate domain.
  // Equal-distance candidates are broken by lexicographic point order.
  // Empty trees return std::nullopt.
  [[nodiscard]] std::optional<KdNearestResult> nearest(Point2i query) const;

  // Replays the static KD-tree structural invariants. This diagnostic is not
  // required for queries and intentionally favors clarity over optimal cost.
  [[nodiscard]] bool valid_structure() const;

 private:
  struct Node {
    Point2i point{};
    std::size_t left{};
    std::size_t right{};
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::uint8_t axis{};
  };

  static constexpr std::size_t no_node = static_cast<std::size_t>(-1);

  std::size_t build(std::vector<Point2i>& points, std::size_t begin,
                    std::size_t end, std::size_t depth);
  [[nodiscard]] std::size_t compute_height(std::size_t node) const noexcept;
  void search(std::size_t node, Point2i query, KdNearestResult& best) const;
  [[nodiscard]] bool validate_subtree(std::size_t node, std::size_t depth,
                                      std::vector<bool>& seen,
                                      std::size_t& count) const;

  std::vector<Node> nodes_;
  std::size_t root_{no_node};
};

}  // namespace algorithms::geometry
