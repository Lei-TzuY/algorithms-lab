#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace algorithms::data_structures {

struct LiChaoQueryResult {
  std::int64_t value;
  std::size_t line_id;

  friend bool operator==(const LiChaoQueryResult&, const LiChaoQueryResult&) =
      default;
};

// Dynamic minimum line envelope over an inclusive integer x-domain.
// Inputs are bounded so every permitted line evaluation is exactly int64_t-
// representable; see docs/scope_recovery_li_chao_tree.md.
class LiChaoMinTree {
 public:
  static constexpr std::int64_t kAbsoluteInputLimit = 1'000'000'000LL;

  LiChaoMinTree(std::int64_t minimum_x, std::int64_t maximum_x);
  LiChaoMinTree(const LiChaoMinTree&) = delete;
  LiChaoMinTree& operator=(const LiChaoMinTree&) = delete;
  LiChaoMinTree(LiChaoMinTree&&) noexcept = default;
  LiChaoMinTree& operator=(LiChaoMinTree&&) noexcept = default;
  ~LiChaoMinTree() = default;

  [[nodiscard]] std::int64_t minimum_x() const noexcept;
  [[nodiscard]] std::int64_t maximum_x() const noexcept;
  [[nodiscard]] std::size_t line_count() const noexcept;
  [[nodiscard]] std::size_t allocated_node_count() const noexcept;

  // Returns a stable insertion-order line id. Slopes and intercepts must lie in
  // [-kAbsoluteInputLimit, kAbsoluteInputLimit].
  std::size_t add_line(std::int64_t slope, std::int64_t intercept);

  // Returns the minimum line value at x. Equal values choose the smallest line
  // id. An empty envelope returns std::nullopt.
  [[nodiscard]] std::optional<LiChaoQueryResult> query(std::int64_t x) const;

 private:
  struct Line {
    std::int64_t slope;
    std::int64_t intercept;
    std::size_t id;
  };

  struct Node {
    std::optional<Line> line;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
  };

  std::int64_t minimum_x_;
  std::int64_t maximum_x_;
  std::unique_ptr<Node> root_;
  std::size_t line_count_ = 0U;
  std::size_t allocated_node_count_ = 0U;

  static void validate_bounded(std::int64_t value, const char* name);
  [[nodiscard]] static std::int64_t evaluate(const Line& line,
                                             std::int64_t x) noexcept;
  [[nodiscard]] static bool better_at(const Line& first, const Line& second,
                                      std::int64_t x) noexcept;
  [[nodiscard]] static std::int64_t midpoint(std::int64_t left,
                                             std::int64_t right) noexcept;

  void insert_line(std::unique_ptr<Node>& node, Line line,
                   std::int64_t left, std::int64_t right);
};

}  // namespace algorithms::data_structures
