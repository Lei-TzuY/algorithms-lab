#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace algorithms::data_structures {

class PersistentSegmentTree {
 public:
  using Version = std::size_t;

  explicit PersistentSegmentTree(std::size_t element_count);
  explicit PersistentSegmentTree(const std::vector<std::int64_t>& values);

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t version_count() const noexcept {
    return roots_.size();
  }
  [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }

  // Creates a new immutable version by assigning one logical element.
  // On any failure, neither version_count() nor node_count() changes.
  Version assign(Version base_version, std::size_t index, std::int64_t value);

  // Returns the exact sum over [begin, end) in one immutable version.
  [[nodiscard]] std::int64_t range_sum(Version version, std::size_t begin,
                                       std::size_t end) const;

 private:
  struct Node {
    std::int64_t sum = 0;
    std::size_t left = null_node();
    std::size_t right = null_node();

    static constexpr std::size_t null_node() noexcept {
      return std::numeric_limits<std::size_t>::max();
    }
  };

  static constexpr std::size_t null_node() noexcept {
    return Node::null_node();
  }

  std::size_t build_zeros(std::size_t begin, std::size_t end);
  std::size_t build_values(const std::vector<std::int64_t>& values,
                           std::size_t begin, std::size_t end);
  std::size_t update_node(std::size_t node, std::size_t begin,
                          std::size_t end, std::size_t index,
                          std::int64_t value);
  void collect_terms(std::size_t node, std::size_t begin, std::size_t end,
                     std::size_t query_begin, std::size_t query_end,
                     std::vector<std::int64_t>& terms) const;

  static std::int64_t checked_add(std::int64_t left, std::int64_t right);
  static std::int64_t checked_sum_terms(std::vector<std::int64_t> terms);
  void validate_version(Version version) const;
  void validate_index(std::size_t index) const;
  [[nodiscard]] std::size_t update_path_nodes() const noexcept;

  std::size_t size_ = 0;
  std::vector<Node> nodes_;
  std::vector<std::size_t> roots_;
};

}  // namespace algorithms::data_structures
