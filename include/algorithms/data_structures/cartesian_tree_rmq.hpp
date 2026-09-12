#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class CartesianTreeRmq {
 public:
  explicit CartesianTreeRmq(std::vector<std::int64_t> values)
      : values_(std::move(values)), parent_(values_.size(), npos()),
        left_(values_.size(), npos()), right_(values_.size(), npos()) {
    const std::size_t max_safe_vertices =
        std::numeric_limits<std::size_t>::max() / 2U + 1U;
    if (values_.size() > max_safe_vertices) {
      throw std::length_error("CartesianTreeRmq Euler tour is not representable");
    }
    build_cartesian_tree();
    build_euler_rmq();
  }

  [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
  [[nodiscard]] bool empty() const noexcept { return values_.empty(); }

  [[nodiscard]] std::optional<std::size_t> root_index() const noexcept {
    if (root_ == npos()) {
      return std::nullopt;
    }
    return root_;
  }

  [[nodiscard]] std::optional<std::size_t> parent_of(std::size_t index) const {
    validate_index(index);
    return optional_index(parent_[index]);
  }

  [[nodiscard]] std::optional<std::size_t> left_child(std::size_t index) const {
    validate_index(index);
    return optional_index(left_[index]);
  }

  [[nodiscard]] std::optional<std::size_t> right_child(std::size_t index) const {
    validate_index(index);
    return optional_index(right_[index]);
  }

  [[nodiscard]] std::size_t range_min_index(std::size_t begin,
                                             std::size_t end) const {
    if (begin > end || end > values_.size()) {
      throw std::out_of_range("CartesianTreeRmq range out of bounds");
    }
    if (begin == end) {
      throw std::invalid_argument("CartesianTreeRmq requires non-empty range");
    }
    const std::size_t first = first_occurrence_[begin];
    const std::size_t last = first_occurrence_[end - 1U];
    const std::size_t euler_position = euler_rmq(first, last);
    return euler_vertices_[euler_position];
  }

  [[nodiscard]] std::int64_t range_min(std::size_t begin,
                                        std::size_t end) const {
    return values_[range_min_index(begin, end)];
  }

  [[nodiscard]] const std::vector<std::int64_t>& values() const noexcept {
    return values_;
  }

  [[nodiscard]] std::size_t euler_size() const noexcept {
    return euler_vertices_.size();
  }

  [[nodiscard]] std::size_t micro_block_size() const noexcept {
    return block_size_;
  }

  [[nodiscard]] std::size_t micro_type_count() const noexcept {
    return micro_tables_.size();
  }

  [[nodiscard]] bool valid_structure() const {
    const std::size_t n = values_.size();
    if (n == 0U) {
      return root_ == npos() && parent_.empty() && left_.empty() && right_.empty() &&
             euler_vertices_.empty() && first_occurrence_.empty();
    }
    if (root_ >= n || parent_[root_] != npos()) {
      return false;
    }

    std::vector<unsigned char> seen(n, 0U);
    std::vector<std::size_t> stack;
    std::size_t current = root_;
    std::size_t expected_inorder = 0U;
    while (current != npos() || !stack.empty()) {
      while (current != npos()) {
        if (current >= n || seen[current] != 0U) {
          return false;
        }
        seen[current] = 1U;
        stack.push_back(current);
        current = left_[current];
      }
      current = stack.back();
      stack.pop_back();
      if (current != expected_inorder) {
        return false;
      }
      ++expected_inorder;
      current = right_[current];
    }
    if (expected_inorder != n) {
      return false;
    }

    for (std::size_t vertex = 0; vertex < n; ++vertex) {
      if (vertex != root_ && parent_[vertex] == npos()) {
        return false;
      }
      for (const std::size_t child : {left_[vertex], right_[vertex]}) {
        if (child == npos()) {
          continue;
        }
        if (child >= n || parent_[child] != vertex) {
          return false;
        }
        if (values_[vertex] > values_[child]) {
          return false;
        }
        if (values_[vertex] == values_[child] && vertex > child) {
          return false;
        }
      }
    }

    const std::size_t expected_euler_size = 2U * n - 1U;
    if (euler_vertices_.size() != expected_euler_size ||
        euler_depths_.size() != euler_vertices_.size() ||
        first_occurrence_.size() != n) {
      return false;
    }
    for (std::size_t i = 0; i < euler_vertices_.size(); ++i) {
      if (euler_vertices_[i] >= n) {
        return false;
      }
      if (i > 0U) {
        const std::size_t a = euler_depths_[i - 1U];
        const std::size_t b = euler_depths_[i];
        if (!(a + 1U == b || b + 1U == a)) {
          return false;
        }
      }
    }
    for (std::size_t vertex = 0; vertex < n; ++vertex) {
      if (first_occurrence_[vertex] >= euler_vertices_.size() ||
          euler_vertices_[first_occurrence_[vertex]] != vertex) {
        return false;
      }
    }
    return true;
  }

 private:
  struct MicroTable {
    std::size_t length{};
    std::uint64_t mask{};
    std::vector<unsigned char> minimum_offsets;
  };

  struct BlockDescriptor {
    std::size_t length{};
    std::uint64_t mask{};

    friend bool operator<(const BlockDescriptor& a,
                          const BlockDescriptor& b) noexcept {
      if (a.length != b.length) {
        return a.length < b.length;
      }
      return a.mask < b.mask;
    }
    friend bool operator==(const BlockDescriptor&, const BlockDescriptor&) = default;
  };

  static constexpr std::size_t npos() noexcept {
    return std::numeric_limits<std::size_t>::max();
  }

  [[nodiscard]] static std::optional<std::size_t> optional_index(
      std::size_t index) noexcept {
    if (index == npos()) {
      return std::nullopt;
    }
    return index;
  }

  void validate_index(std::size_t index) const {
    if (index >= values_.size()) {
      throw std::out_of_range("CartesianTreeRmq index out of bounds");
    }
  }

  void build_cartesian_tree() {
    const std::size_t n = values_.size();
    if (n == 0U) {
      return;
    }

    std::vector<std::size_t> stack;
    stack.reserve(n);
    for (std::size_t index = 0; index < n; ++index) {
      std::size_t last_popped = npos();
      while (!stack.empty() && values_[index] < values_[stack.back()]) {
        last_popped = stack.back();
        stack.pop_back();
      }
      if (!stack.empty()) {
        parent_[index] = stack.back();
        right_[stack.back()] = index;
      }
      if (last_popped != npos()) {
        parent_[last_popped] = index;
        left_[index] = last_popped;
      }
      stack.push_back(index);
    }
    root_ = stack.front();
  }

  void record_euler(std::size_t vertex, std::size_t depth) {
    if (first_occurrence_[vertex] == npos()) {
      first_occurrence_[vertex] = euler_vertices_.size();
    }
    euler_vertices_.push_back(vertex);
    euler_depths_.push_back(depth);
  }

  void build_euler_tour() {
    const std::size_t n = values_.size();
    first_occurrence_.assign(n, npos());
    if (n == 0U) {
      return;
    }

    struct Frame {
      std::size_t vertex;
      std::size_t depth;
      unsigned char state;
    };
    std::vector<Frame> stack;
    stack.reserve(n);
    stack.push_back(Frame{root_, 0U, 0U});
    while (!stack.empty()) {
      Frame& frame = stack.back();
      if (frame.state == 0U) {
        record_euler(frame.vertex, frame.depth);
        frame.state = 1U;
        const std::size_t child = left_[frame.vertex];
        if (child != npos()) {
          stack.push_back(Frame{child, frame.depth + 1U, 0U});
        }
        continue;
      }
      if (frame.state == 1U) {
        if (left_[frame.vertex] != npos()) {
          record_euler(frame.vertex, frame.depth);
        }
        frame.state = 2U;
        const std::size_t child = right_[frame.vertex];
        if (child != npos()) {
          stack.push_back(Frame{child, frame.depth + 1U, 0U});
        }
        continue;
      }
      if (right_[frame.vertex] != npos()) {
        record_euler(frame.vertex, frame.depth);
      }
      stack.pop_back();
    }
  }

  [[nodiscard]] std::size_t better_euler_position(std::size_t a,
                                                   std::size_t b) const noexcept {
    if (euler_depths_[a] != euler_depths_[b]) {
      return euler_depths_[a] < euler_depths_[b] ? a : b;
    }
    return a < b ? a : b;
  }

  [[nodiscard]] BlockDescriptor describe_block(std::size_t block) const {
    const std::size_t start = block * block_size_;
    const std::size_t length = std::min(block_size_, euler_depths_.size() - start);
    std::uint64_t mask = 0U;
    for (std::size_t offset = 1U; offset < length; ++offset) {
      if (euler_depths_[start + offset] > euler_depths_[start + offset - 1U]) {
        mask |= (std::uint64_t{1} << (offset - 1U));
      }
    }
    return BlockDescriptor{length, mask};
  }

  [[nodiscard]] static MicroTable build_micro_table(const BlockDescriptor& type) {
    std::vector<int> relative_depth(type.length, 0);
    for (std::size_t i = 1; i < type.length; ++i) {
      const bool rises = ((type.mask >> (i - 1U)) & std::uint64_t{1}) != 0U;
      relative_depth[i] = relative_depth[i - 1U] + (rises ? 1 : -1);
    }
    std::vector<unsigned char> table(type.length * type.length, 0U);
    for (std::size_t begin = 0; begin < type.length; ++begin) {
      std::size_t best = begin;
      for (std::size_t end = begin; end < type.length; ++end) {
        if (relative_depth[end] < relative_depth[best]) {
          best = end;
        }
        table[begin * type.length + end] = static_cast<unsigned char>(best);
      }
    }
    return MicroTable{type.length, type.mask, std::move(table)};
  }

  [[nodiscard]] std::size_t micro_query(std::size_t block,
                                        std::size_t begin_offset,
                                        std::size_t end_offset) const {
    const MicroTable& table = micro_tables_[block_micro_index_[block]];
    const std::size_t offset = table.minimum_offsets[
        begin_offset * table.length + end_offset];
    return block * block_size_ + offset;
  }

  void build_microblocks() {
    const std::size_t m = euler_depths_.size();
    if (m == 0U) {
      return;
    }
    const std::size_t floor_log =
        static_cast<std::size_t>(std::bit_width(m) - 1);
    block_size_ = std::max<std::size_t>(1U, floor_log / 2U);
    const std::size_t block_count = (m + block_size_ - 1U) / block_size_;

    block_types_.reserve(block_count);
    for (std::size_t block = 0; block < block_count; ++block) {
      block_types_.push_back(describe_block(block));
    }

    std::vector<BlockDescriptor> unique_types = block_types_;
    std::sort(unique_types.begin(), unique_types.end());
    unique_types.erase(std::unique(unique_types.begin(), unique_types.end()),
                       unique_types.end());
    micro_tables_.reserve(unique_types.size());
    for (const BlockDescriptor& type : unique_types) {
      micro_tables_.push_back(build_micro_table(type));
    }

    block_micro_index_.resize(block_count);
    block_minimum_euler_.resize(block_count);
    for (std::size_t block = 0; block < block_count; ++block) {
      const auto it = std::lower_bound(unique_types.begin(), unique_types.end(),
                                       block_types_[block]);
      block_micro_index_[block] = static_cast<std::size_t>(it - unique_types.begin());
      block_minimum_euler_[block] =
          micro_query(block, 0U, block_types_[block].length - 1U);
    }
  }

  void build_block_sparse_table() {
    const std::size_t count = block_minimum_euler_.size();
    block_logs_.assign(count + 1U, 0U);
    for (std::size_t i = 2U; i <= count; ++i) {
      block_logs_[i] = block_logs_[i / 2U] + 1U;
    }
    if (count == 0U) {
      return;
    }
    const std::size_t levels = block_logs_[count] + 1U;
    block_sparse_.assign(levels, std::vector<std::size_t>(count, 0U));
    block_sparse_[0] = block_minimum_euler_;
    for (std::size_t level = 1U; level < levels; ++level) {
      const std::size_t span = std::size_t{1} << level;
      const std::size_t half = span / 2U;
      for (std::size_t i = 0; i + span <= count; ++i) {
        block_sparse_[level][i] = better_euler_position(
            block_sparse_[level - 1U][i],
            block_sparse_[level - 1U][i + half]);
      }
    }
  }

  void build_euler_rmq() {
    build_euler_tour();
    build_microblocks();
    build_block_sparse_table();
  }

  [[nodiscard]] std::size_t full_block_query(std::size_t begin_block,
                                              std::size_t end_block) const {
    const std::size_t length = end_block - begin_block + 1U;
    const std::size_t level = block_logs_[length];
    const std::size_t span = std::size_t{1} << level;
    return better_euler_position(block_sparse_[level][begin_block],
                                 block_sparse_[level][end_block + 1U - span]);
  }

  [[nodiscard]] std::size_t euler_rmq(std::size_t first,
                                       std::size_t second) const {
    std::size_t left_pos = first;
    std::size_t right_pos = second;
    if (left_pos > right_pos) {
      std::swap(left_pos, right_pos);
    }
    const std::size_t left_block = left_pos / block_size_;
    const std::size_t right_block = right_pos / block_size_;
    if (left_block == right_block) {
      return micro_query(left_block, left_pos % block_size_,
                         right_pos % block_size_);
    }

    std::size_t best = micro_query(left_block, left_pos % block_size_,
                                   block_types_[left_block].length - 1U);
    best = better_euler_position(
        best, micro_query(right_block, 0U, right_pos % block_size_));
    if (left_block + 1U < right_block) {
      best = better_euler_position(
          best, full_block_query(left_block + 1U, right_block - 1U));
    }
    return best;
  }

  std::vector<std::int64_t> values_;
  std::vector<std::size_t> parent_;
  std::vector<std::size_t> left_;
  std::vector<std::size_t> right_;
  std::size_t root_{npos()};

  std::vector<std::size_t> euler_vertices_;
  std::vector<std::size_t> euler_depths_;
  std::vector<std::size_t> first_occurrence_;

  std::size_t block_size_{1U};
  std::vector<BlockDescriptor> block_types_;
  std::vector<std::size_t> block_micro_index_;
  std::vector<MicroTable> micro_tables_;
  std::vector<std::size_t> block_minimum_euler_;
  std::vector<std::size_t> block_logs_;
  std::vector<std::vector<std::size_t>> block_sparse_;
};

}  // namespace algorithms::data_structures
