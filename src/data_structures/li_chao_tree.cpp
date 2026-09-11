#include "algorithms/data_structures/li_chao_tree.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace algorithms::data_structures {

LiChaoMinTree::LiChaoMinTree(std::int64_t minimum_x,
                             std::int64_t maximum_x)
    : minimum_x_(minimum_x), maximum_x_(maximum_x) {
  validate_bounded(minimum_x_, "minimum x");
  validate_bounded(maximum_x_, "maximum x");
  if (minimum_x_ > maximum_x_) {
    throw std::invalid_argument("Li Chao domain minimum exceeds maximum");
  }
}

std::int64_t LiChaoMinTree::minimum_x() const noexcept { return minimum_x_; }

std::int64_t LiChaoMinTree::maximum_x() const noexcept { return maximum_x_; }

std::size_t LiChaoMinTree::line_count() const noexcept { return line_count_; }

std::size_t LiChaoMinTree::allocated_node_count() const noexcept {
  return allocated_node_count_;
}

std::size_t LiChaoMinTree::add_line(std::int64_t slope,
                                    std::int64_t intercept) {
  validate_bounded(slope, "slope");
  validate_bounded(intercept, "intercept");
  if (line_count_ == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("Li Chao line id space exhausted");
  }

  const std::size_t id = line_count_;
  insert_line(root_, Line{slope, intercept, id}, minimum_x_, maximum_x_);
  ++line_count_;
  return id;
}

std::optional<LiChaoQueryResult> LiChaoMinTree::query(std::int64_t x) const {
  if (x < minimum_x_ || x > maximum_x_) {
    throw std::out_of_range("Li Chao query x is outside the configured domain");
  }

  const Node* node = root_.get();
  std::int64_t left = minimum_x_;
  std::int64_t right = maximum_x_;
  std::optional<LiChaoQueryResult> best;

  while (node != nullptr) {
    if (node->line.has_value()) {
      const LiChaoQueryResult candidate{evaluate(*node->line, x),
                                        node->line->id};
      if (!best.has_value() || candidate.value < best->value ||
          (candidate.value == best->value &&
           candidate.line_id < best->line_id)) {
        best = candidate;
      }
    }

    if (left == right) {
      break;
    }
    const std::int64_t middle = midpoint(left, right);
    if (x <= middle) {
      node = node->left.get();
      right = middle;
    } else {
      node = node->right.get();
      left = middle + 1;
    }
  }

  return best;
}

void LiChaoMinTree::validate_bounded(std::int64_t value, const char* name) {
  if (value < -kAbsoluteInputLimit || value > kAbsoluteInputLimit) {
    throw std::out_of_range(std::string("Li Chao ") + name +
                            " exceeds the exact arithmetic domain");
  }
}

std::int64_t LiChaoMinTree::evaluate(const Line& line,
                                     std::int64_t x) noexcept {
  // Both factors and the intercept are bounded by 1e9. Therefore
  // |slope*x + intercept| <= 1,000,000,001,000,000,000 < INT64_MAX.
  return line.slope * x + line.intercept;
}

bool LiChaoMinTree::better_at(const Line& first, const Line& second,
                              std::int64_t x) noexcept {
  const std::int64_t first_value = evaluate(first, x);
  const std::int64_t second_value = evaluate(second, x);
  return first_value < second_value ||
         (first_value == second_value && first.id < second.id);
}

std::int64_t LiChaoMinTree::midpoint(std::int64_t left,
                                     std::int64_t right) noexcept {
  // The validated domain is within [-1e9, 1e9], so right-left is safe.
  return left + (right - left) / 2;
}

void LiChaoMinTree::insert_line(std::unique_ptr<Node>& node, Line line,
                                std::int64_t left, std::int64_t right) {
  if (node == nullptr) {
    node = std::make_unique<Node>();
    ++allocated_node_count_;
  }
  if (!node->line.has_value()) {
    node->line = line;
    return;
  }

  const std::int64_t middle = midpoint(left, right);
  if (better_at(line, *node->line, middle)) {
    std::swap(line, *node->line);
  }

  if (left == right) {
    return;
  }

  // The line left in `line` is not better at the midpoint. Two affine lines
  // cross at most once, so it can still win on at most one endpoint side.
  if (better_at(line, *node->line, left)) {
    insert_line(node->left, line, left, middle);
  } else if (better_at(line, *node->line, right)) {
    insert_line(node->right, line, middle + 1, right);
  }
}

}  // namespace algorithms::data_structures
