#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

struct MinimumDominatingSetResult {
  std::vector<Vertex> vertices;
  std::uint64_t explored_states{};
  std::uint64_t bound_prunes{};
};

namespace detail {

class DominatingSetSearch {
 public:
  explicit DominatingSetSearch(const Graph& graph)
      : vertex_count_(graph.vertex_count()), all_mask_(vertex_count_ == 0 ? 0ULL : ((1ULL << vertex_count_) - 1ULL)),
        closed_(vertex_count_, 0ULL), seen_(std::size_t{1} << vertex_count_, 0U),
        best_size_(vertex_count_ + 1) {
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      std::uint64_t mask = bit(vertex);
      for (const Edge& edge : graph.neighbors(vertex)) {
        mask |= bit(edge.to);
      }
      closed_[vertex] = mask;
    }
  }

  [[nodiscard]] MinimumDominatingSetResult solve() {
    if (vertex_count_ == 0) {
      return {};
    }

    // Deterministic greedy incumbent gives branch-and-bound a useful finite cap.
    std::uint64_t dominated = 0;
    while (dominated != all_mask_) {
      Vertex best = 0;
      std::size_t best_gain = 0;
      for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
        const std::size_t gain = static_cast<std::size_t>(std::popcount(closed_[vertex] & ~dominated));
        if (gain > best_gain) {
          best_gain = gain;
          best = vertex;
        }
      }
      greedy_incumbent_.push_back(best);
      dominated |= closed_[best];
    }
    best_vertices_ = greedy_incumbent_;
    best_size_ = best_vertices_.size();

    current_.clear();
    search(0ULL, 0ULL);
    std::sort(best_vertices_.begin(), best_vertices_.end());
    return MinimumDominatingSetResult{best_vertices_, explored_states_, bound_prunes_};
  }

 private:
  [[nodiscard]] static std::uint64_t bit(Vertex vertex) { return 1ULL << vertex; }

  [[nodiscard]] std::size_t lower_bound_additional(std::uint64_t dominated,
                                                    std::uint64_t selected) const {
    const std::uint64_t remaining = all_mask_ & ~dominated;
    const std::size_t remaining_count = static_cast<std::size_t>(std::popcount(remaining));
    if (remaining_count == 0) {
      return 0;
    }
    std::size_t max_gain = 0;
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      if ((selected & bit(vertex)) != 0) {
        continue;
      }
      max_gain = std::max(max_gain,
                          static_cast<std::size_t>(std::popcount(closed_[vertex] & remaining)));
    }
    if (max_gain == 0) {
      return vertex_count_ + 1;
    }
    return (remaining_count + max_gain - 1) / max_gain;
  }

  [[nodiscard]] Vertex choose_branch_vertex(std::uint64_t dominated) const {
    Vertex chosen = vertex_count_;
    std::size_t chosen_candidates = std::numeric_limits<std::size_t>::max();
    const std::uint64_t remaining = all_mask_ & ~dominated;
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      if ((remaining & bit(vertex)) == 0) {
        continue;
      }
      const std::size_t candidates = static_cast<std::size_t>(std::popcount(closed_[vertex]));
      if (candidates < chosen_candidates) {
        chosen_candidates = candidates;
        chosen = vertex;
      }
    }
    return chosen;
  }

  void search(std::uint64_t dominated, std::uint64_t selected) {
    const std::size_t state = static_cast<std::size_t>(selected);
    if (seen_[state] != 0U) {
      return;
    }
    seen_[state] = 1U;
    ++explored_states_;
    if (dominated == all_mask_) {
      if (current_.size() < best_size_) {
        best_size_ = current_.size();
        best_vertices_ = current_;
      }
      return;
    }
    if (current_.size() >= best_size_) {
      ++bound_prunes_;
      return;
    }
    const std::size_t bound = lower_bound_additional(dominated, selected);
    if (bound > vertex_count_ || current_.size() + bound >= best_size_) {
      ++bound_prunes_;
      return;
    }

    const Vertex witness = choose_branch_vertex(dominated);
    if (witness == vertex_count_) {
      return;
    }

    std::vector<Vertex> candidates;
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      if ((closed_[witness] & bit(vertex)) != 0 && (selected & bit(vertex)) == 0) {
        candidates.push_back(vertex);
      }
    }

    // Prefer larger immediate gain; vertex id breaks ties for replayability.
    std::sort(candidates.begin(), candidates.end(), [&](Vertex left, Vertex right) {
      const auto left_gain = std::popcount(closed_[left] & ~dominated);
      const auto right_gain = std::popcount(closed_[right] & ~dominated);
      if (left_gain != right_gain) {
        return left_gain > right_gain;
      }
      return left < right;
    });

    for (const Vertex candidate : candidates) {
      current_.push_back(candidate);
      search(dominated | closed_[candidate], selected | bit(candidate));
      current_.pop_back();
    }
  }

  std::size_t vertex_count_;
  std::uint64_t all_mask_;
  std::vector<std::uint64_t> closed_;
  std::vector<unsigned char> seen_;
  std::vector<Vertex> current_;
  std::vector<Vertex> greedy_incumbent_;
  std::vector<Vertex> best_vertices_;
  std::size_t best_size_;
  std::uint64_t explored_states_{};
  std::uint64_t bound_prunes_{};
};

}  // namespace detail

[[nodiscard]] inline MinimumDominatingSetResult minimum_dominating_set(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("minimum dominating set requires an undirected graph");
  }
  constexpr std::size_t kExactVertexLimit = 24;
  if (graph.vertex_count() > kExactVertexLimit) {
    throw std::length_error("minimum dominating set exact baseline supports at most 24 vertices");
  }
  return detail::DominatingSetSearch(graph).solve();
}

}  // namespace algorithms::graphs
