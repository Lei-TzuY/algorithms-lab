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

  // Returns the zero-based rank-th vertex on the represented path from first to
  // second. Deferred reverse/assignment/addition state is pushed while
  // descending the exposed auxiliary splay, and the selected vertex is splayed
  // before return. Throws std::invalid_argument when disconnected and
  // std::out_of_range when rank is outside the represented path.
  [[nodiscard]] Vertex kth_vertex_on_path(Vertex first, Vertex second,
                                          std::size_t rank);

  // Returns the number of vertices in vertex's represented subtree when the
  // containing tree is rooted at root. Preferred-path structure may change;
  // represented topology and all numeric values remain unchanged. Throws
  // std::invalid_argument when root and vertex are disconnected.
  [[nodiscard]] std::size_t rooted_subtree_vertex_count(Vertex root,
                                                         Vertex vertex);

  // Replaces one vertex value exactly. The operation itself never rejects a
  // representable int64 value merely because an exposed auxiliary aggregate is
  // outside int64; internal aggregates use a wider exact representation.
  void assign_value(Vertex vertex, std::int64_t value);

  // Uniformly replaces every node value on the represented path. The exposed
  // preferred path receives one lazy assignment tag, so the operation preserves
  // exact aggregate state without narrowing intermediate sums.
  void assign_path_value(Vertex first, Vertex second, std::int64_t value);

  // Adds delta to every node value on the represented path. The operation
  // rejects transactionally before changing any represented node value if even
  // one affected value would leave int64. Preferred-path exposure may still
  // rearrange auxiliary splay structure, as with all link-cut queries.
  void add_path_value(Vertex first, Vertex second, std::int64_t delta);

  // Returns the exact represented-path node-value sum when it is representable
  // as int64. Throws std::overflow_error only when that final exact result is
  // genuinely outside int64, and std::invalid_argument when disconnected.
  [[nodiscard]] std::int64_t path_sum(Vertex first, Vertex second);

  // Diagnostic check for the auxiliary forest: child/parent consistency,
  // acyclicity, stored auxiliary subtree sizes, represented-cardinality
  // arithmetic, and exact aggregate/lazy-tag semantics. A parent pointer may be
  // a represented path-parent even when it is not an auxiliary-tree child link.
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
    std::size_t virtual_size = 0;
    std::size_t represented_size = 1;
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
  [[nodiscard]] std::size_t child_represented_size(Vertex vertex) const noexcept;
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
