#include "algorithms/graphs/bipartite_matching.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

constexpr std::size_t kUnmatched = std::numeric_limits<std::size_t>::max();

class HopcroftKarpSolver {
 public:
  HopcroftKarpSolver(std::size_t left_count, std::size_t right_count,
                     std::span<const BipartiteEdge> edges)
      : adjacency_(left_count),
        left_match_(left_count, kUnmatched),
        right_match_(right_count, kUnmatched),
        distance_(left_count, kUnmatched) {
    for (const auto& edge : edges) {
      if (edge.left >= left_count || edge.right >= right_count) {
        throw std::out_of_range("bipartite edge endpoint is out of range");
      }
      adjacency_[edge.left].push_back(edge.right);
    }
  }

  [[nodiscard]] BipartiteMatchingResult solve() {
    std::size_t cardinality = 0;
    while (build_layers()) {
      for (std::size_t left = 0; left < left_match_.size(); ++left) {
        if (left_match_[left] == kUnmatched && augment(left)) {
          ++cardinality;
        }
      }
    }

    auto [left_cover, right_cover] = minimum_vertex_cover();
    std::vector<std::optional<std::size_t>> left_output(left_match_.size());
    std::vector<std::optional<std::size_t>> right_output(right_match_.size());
    for (std::size_t left = 0; left < left_match_.size(); ++left) {
      if (left_match_[left] != kUnmatched) {
        left_output[left] = left_match_[left];
      }
    }
    for (std::size_t right = 0; right < right_match_.size(); ++right) {
      if (right_match_[right] != kUnmatched) {
        right_output[right] = right_match_[right];
      }
    }
    return BipartiteMatchingResult{cardinality, std::move(left_output),
                                   std::move(right_output),
                                   std::move(left_cover),
                                   std::move(right_cover)};
  }

 private:
  [[nodiscard]] bool build_layers() {
    std::fill(distance_.begin(), distance_.end(), kUnmatched);
    shortest_augmenting_length_ = kUnmatched;
    std::queue<std::size_t> queue;
    for (std::size_t left = 0; left < left_match_.size(); ++left) {
      if (left_match_[left] == kUnmatched) {
        distance_[left] = 0;
        queue.push(left);
      }
    }

    while (!queue.empty()) {
      const std::size_t left = queue.front();
      queue.pop();
      if (distance_[left] >= shortest_augmenting_length_) {
        continue;
      }
      for (const std::size_t right : adjacency_[left]) {
        const std::size_t matched_left = right_match_[right];
        if (matched_left == kUnmatched) {
          shortest_augmenting_length_ = distance_[left] + 1;
        } else if (distance_[matched_left] == kUnmatched) {
          distance_[matched_left] = distance_[left] + 1;
          queue.push(matched_left);
        }
      }
    }
    return shortest_augmenting_length_ != kUnmatched;
  }

  [[nodiscard]] bool augment(std::size_t left) {
    for (const std::size_t right : adjacency_[left]) {
      const std::size_t matched_left = right_match_[right];
      if (matched_left == kUnmatched) {
        if (distance_[left] + 1 != shortest_augmenting_length_) {
          continue;
        }
      } else {
        if (distance_[matched_left] != distance_[left] + 1 ||
            !augment(matched_left)) {
          continue;
        }
      }
      left_match_[left] = right;
      right_match_[right] = left;
      return true;
    }
    distance_[left] = kUnmatched;
    return false;
  }

  [[nodiscard]] std::pair<std::vector<bool>, std::vector<bool>>
  minimum_vertex_cover() const {
    std::vector<bool> reachable_left(left_match_.size(), false);
    std::vector<bool> reachable_right(right_match_.size(), false);
    std::queue<std::size_t> queue;
    for (std::size_t left = 0; left < left_match_.size(); ++left) {
      if (left_match_[left] == kUnmatched) {
        reachable_left[left] = true;
        queue.push(left);
      }
    }

    while (!queue.empty()) {
      const std::size_t left = queue.front();
      queue.pop();
      for (const std::size_t right : adjacency_[left]) {
        if (left_match_[left] == right || reachable_right[right]) {
          continue;
        }
        reachable_right[right] = true;
        const std::size_t matched_left = right_match_[right];
        if (matched_left != kUnmatched && !reachable_left[matched_left]) {
          reachable_left[matched_left] = true;
          queue.push(matched_left);
        }
      }
    }

    std::vector<bool> left_cover(left_match_.size(), false);
    for (std::size_t left = 0; left < left_cover.size(); ++left) {
      left_cover[left] = !reachable_left[left];
    }
    return {std::move(left_cover), std::move(reachable_right)};
  }

  std::vector<std::vector<std::size_t>> adjacency_;
  std::vector<std::size_t> left_match_;
  std::vector<std::size_t> right_match_;
  std::vector<std::size_t> distance_;
  std::size_t shortest_augmenting_length_{kUnmatched};
};

}  // namespace

BipartiteMatchingResult hopcroft_karp(std::size_t left_count,
                                      std::size_t right_count,
                                      std::span<const BipartiteEdge> edges) {
  return HopcroftKarpSolver(left_count, right_count, edges).solve();
}

}  // namespace algorithms::graphs
