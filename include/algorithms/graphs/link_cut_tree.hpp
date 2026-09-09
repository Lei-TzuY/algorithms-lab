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

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return nodes_.size();
  }

  // Adds one represented-tree edge. Throws when the edge would be a self-loop
  // or would connect vertices already in the same represented tree.
  void link(Vertex first, Vertex second);

  // Removes exactly one represented-tree edge. Throws when the requested pair
  // is not a direct represented-tree edge.
  void cut(Vertex first, Vertex second);

  // Link-cut queries are structurally mutating: preferred paths are exposed and
  // auxiliary splay trees are rearranged even though represented topology stays
  // unchanged.
  [[nodiscard]] bool connected(Vertex first, Vertex second);

  // Returns the number of represented-tree edges on the unique path.
  // Throws when the vertices are disconnected.
  [[nodiscard]] std::size_t path_edge_distance(Vertex first, Vertex second);

  // Replaces one vertex value exactly. The operation itself never rejects a
  // representable int64 value merely because an exposed auxiliary aggregate is
  // outside int64; internal aggregates use a wider exact representation.
  void assign_value(Vertex vertex, std::int64_t value);

  // Returns the exact represented-path node-value sum when it is representable
  // as int64. Throws std::overflow_error only when that final exact result is
  // genuinely outside int64, and std::invalid_argument when disconnected.
  [[nodiscard]] std::int64_t path_sum(Vertex first, Vertex second);

  // Diagnostic check for the auxiliary forest: child/parent consistency,
  // acyclicity, stored auxiliary subtree sizes, and stored exact aggregates. A
  // parent pointer may be a represented path-parent even when it is not an
  // auxiliary-tree child link.
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
    bool reversed = false;
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
  static bool exact_equal(const ExactSum& first,
                          const ExactSum& second) noexcept;
  static std::int64_t narrow_exact(const ExactSum& value);
  void pull(Vertex vertex) noexcept;
  void apply_reverse(Vertex vertex) noexcept;
  void push(Vertex vertex) noexcept;
  void push_path(Vertex vertex);
  void rotate(Vertex vertex) noexcept;
  void splay(Vertex vertex);
  void access(Vertex vertex);
  void make_root(Vertex vertex);
  [[nodiscard]] Vertex find_root(Vertex vertex);
};

}  // namespace algorithms::graphs
