#include "algorithms/combinatorial/exact_cover.hpp"

#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {
namespace {

class DancingLinksSolver {
 public:
  DancingLinksSolver(std::size_t column_count,
                     const std::vector<std::vector<std::size_t>>& rows)
      : column_count_(column_count) {
    if (column_count_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("exact-cover column count is too large");
    }

    nodes_.resize(column_count_ + 1);
    column_sizes_.assign(column_count_ + 1, 0);
    initialize_headers();
    append_rows(rows);
  }

  [[nodiscard]] std::optional<ExactCoverSolution> solve() {
    if (!search()) {
      return std::nullopt;
    }
    return ExactCoverSolution{solution_rows_};
  }

 private:
  struct Node {
    std::size_t left = 0;
    std::size_t right = 0;
    std::size_t up = 0;
    std::size_t down = 0;
    std::size_t column = 0;
    std::size_t row = 0;
  };

  static constexpr std::size_t kRoot = 0;

  void initialize_headers() {
    if (column_count_ == 0) {
      nodes_[kRoot].left = kRoot;
      nodes_[kRoot].right = kRoot;
      nodes_[kRoot].up = kRoot;
      nodes_[kRoot].down = kRoot;
      nodes_[kRoot].column = kRoot;
      return;
    }

    nodes_[kRoot].left = column_count_;
    nodes_[kRoot].right = 1;
    nodes_[kRoot].up = kRoot;
    nodes_[kRoot].down = kRoot;
    nodes_[kRoot].column = kRoot;

    for (std::size_t column = 1; column <= column_count_; ++column) {
      Node& header = nodes_[column];
      header.left = (column == 1) ? kRoot : column - 1;
      header.right = (column == column_count_) ? kRoot : column + 1;
      header.up = column;
      header.down = column;
      header.column = column;
    }
  }

  void append_rows(const std::vector<std::vector<std::size_t>>& rows) {
    const std::size_t kUnseen = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> seen(column_count_, kUnseen);

    for (std::size_t row = 0; row < rows.size(); ++row) {
      std::size_t first = kRoot;
      std::size_t previous = kRoot;

      for (const std::size_t zero_based_column : rows[row]) {
        if (zero_based_column >= column_count_) {
          throw std::out_of_range("exact-cover column index out of range");
        }
        if (seen[zero_based_column] == row) {
          throw std::invalid_argument(
              "exact-cover row contains a duplicate column");
        }
        seen[zero_based_column] = row;

        const std::size_t header = zero_based_column + 1;
        const std::size_t node_index = nodes_.size();
        nodes_.push_back(Node{});
        Node& node = nodes_.back();
        node.column = header;
        node.row = row;

        node.up = nodes_[header].up;
        node.down = header;
        nodes_[nodes_[header].up].down = node_index;
        nodes_[header].up = node_index;
        ++column_sizes_[header];

        if (first == kRoot) {
          first = node_index;
          previous = node_index;
          node.left = node_index;
          node.right = node_index;
        } else {
          node.left = previous;
          node.right = first;
          nodes_[previous].right = node_index;
          nodes_[first].left = node_index;
          previous = node_index;
        }
      }
    }
  }

  [[nodiscard]] std::size_t choose_column() const {
    std::size_t best = nodes_[kRoot].right;
    for (std::size_t column = nodes_[best].right; column != kRoot;
         column = nodes_[column].right) {
      if (column_sizes_[column] < column_sizes_[best]) {
        best = column;
      }
    }
    return best;
  }

  void cover(std::size_t column) {
    nodes_[nodes_[column].right].left = nodes_[column].left;
    nodes_[nodes_[column].left].right = nodes_[column].right;

    for (std::size_t row_node = nodes_[column].down; row_node != column;
         row_node = nodes_[row_node].down) {
      for (std::size_t node = nodes_[row_node].right; node != row_node;
           node = nodes_[node].right) {
        nodes_[nodes_[node].down].up = nodes_[node].up;
        nodes_[nodes_[node].up].down = nodes_[node].down;
        --column_sizes_[nodes_[node].column];
      }
    }
  }

  void uncover(std::size_t column) {
    for (std::size_t row_node = nodes_[column].up; row_node != column;
         row_node = nodes_[row_node].up) {
      for (std::size_t node = nodes_[row_node].left; node != row_node;
           node = nodes_[node].left) {
        ++column_sizes_[nodes_[node].column];
        nodes_[nodes_[node].down].up = node;
        nodes_[nodes_[node].up].down = node;
      }
    }

    nodes_[nodes_[column].right].left = column;
    nodes_[nodes_[column].left].right = column;
  }

  [[nodiscard]] bool search() {
    if (nodes_[kRoot].right == kRoot) {
      return true;
    }

    const std::size_t column = choose_column();
    if (column_sizes_[column] == 0) {
      return false;
    }

    cover(column);
    for (std::size_t row_node = nodes_[column].down; row_node != column;
         row_node = nodes_[row_node].down) {
      solution_rows_.push_back(nodes_[row_node].row);

      for (std::size_t node = nodes_[row_node].right; node != row_node;
           node = nodes_[node].right) {
        cover(nodes_[node].column);
      }

      const bool found = search();

      for (std::size_t node = nodes_[row_node].left; node != row_node;
           node = nodes_[node].left) {
        uncover(nodes_[node].column);
      }

      if (found) {
        uncover(column);
        return true;
      }
      solution_rows_.pop_back();
    }
    uncover(column);
    return false;
  }

  std::size_t column_count_;
  std::vector<Node> nodes_;
  std::vector<std::size_t> column_sizes_;
  std::vector<std::size_t> solution_rows_;
};

}  // namespace

std::optional<ExactCoverSolution> solve_exact_cover(
    std::size_t column_count,
    const std::vector<std::vector<std::size_t>>& rows) {
  DancingLinksSolver solver(column_count, rows);
  return solver.solve();
}

}  // namespace algorithms::combinatorial
