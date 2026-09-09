#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace algorithms::graphs {

class LinkCutForest {
 public:
  explicit LinkCutForest(std::size_t vertex_count);

  [[nodiscard]] std::size_t vertex_count() const noexcept { return nodes_.size(); }

  void link(Vertex first, Vertex second);
  void cut(Vertex first, Vertex second);
  [[nodiscard]] bool connected(Vertex first, Vertex second);
  [[nodiscard]] std::size_t path_edge_distance(Vertex first, Vertex second);

  void assign_value(Vertex vertex, std::int64_t value);
  void assign_path_value(Vertex first, Vertex second, std::int64_t value);

  // Adds delta to every represented-path node. Numeric state is transactional:
  // std::overflow_error is thrown before any represented node value changes if
  // even one affected value would leave int64_t. Preferred-path exposure may
  // still rearrange auxiliary splay structure, as with all link-cut queries.
  void add_path_value(Vertex first, Vertex second, std::int64_t delta);

  [[nodiscard]] std::int64_t path_sum(Vertex first, Vertex second);
  [[nodiscard]] bool valid_auxiliary_invariants() const;

 private:
  static constexpr Vertex kNone = std::numeric_limits<Vertex>::max();

  struct ExactSum {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    bool negative = false;
  };

  struct Node {
    Vertex parent = kNone;
    Vertex left = kNone;
    Vertex right = kNone;
    std::size_t auxiliary_size = 1;
    std::int64_t value = 0;
    ExactSum auxiliary_sum{};
    std::int64_t auxiliary_min = 0;
    std::int64_t auxiliary_max = 0;
    bool reversed = false;
    bool has_assignment = false;
    std::int64_t assignment_value = 0;
    ExactSum pending_addition{};
  };

  std::vector<Node> nodes_;

  void validate_vertex(Vertex vertex) const;
  [[nodiscard]] bool is_auxiliary_root(Vertex vertex) const noexcept;
  [[nodiscard]] std::size_t child_size(Vertex vertex) const noexcept;
  [[nodiscard]] ExactSum child_sum(Vertex vertex) const noexcept;
  static ExactSum exact_from_value(std::int64_t value) noexcept;
  static int compare_magnitude(const ExactSum& first,
                               const ExactSum& second) noexcept;
  static ExactSum add_magnitude(const ExactSum& first,
                                const ExactSum& second) noexcept;
  static ExactSum subtract_magnitude(const ExactSum& larger,
                                     const ExactSum& smaller) noexcept;
  static ExactSum add_exact(const ExactSum& first,
                            const ExactSum& second) noexcept;
  static ExactSum scale_exact(const ExactSum& value,
                              std::size_t count) noexcept;
  static ExactSum scale_exact_value(std::int64_t value,
                                    std::size_t count) noexcept;
  static bool exact_equal(const ExactSum& first,
                          const ExactSum& second) noexcept;
  static bool exact_is_zero(const ExactSum& value) noexcept;
  static bool exact_fits_int64(const ExactSum& value) noexcept;
  static std::int64_t narrow_exact_unchecked(const ExactSum& value) noexcept;
  static std::int64_t narrow_exact(const ExactSum& value);
  static bool can_add_int64(std::int64_t value, std::int64_t delta) noexcept;
  static std::int64_t add_exact_to_int64(std::int64_t value,
                                         const ExactSum& delta);

  void pull(Vertex vertex);
  void apply_assignment(Vertex vertex, std::int64_t value) noexcept;
  void apply_addition(Vertex vertex, const ExactSum& delta);
  void apply_reverse(Vertex vertex) noexcept;
  void push(Vertex vertex);
  void push_path(Vertex vertex);
  void rotate(Vertex vertex);
  void splay(Vertex vertex);
  void access(Vertex vertex);
  void make_root(Vertex vertex);
  [[nodiscard]] Vertex find_root(Vertex vertex);
};

}  // namespace algorithms::graphs
