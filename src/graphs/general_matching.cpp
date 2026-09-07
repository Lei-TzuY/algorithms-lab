#include "algorithms/graphs/general_matching.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

class BlossomSolver {
 public:
  explicit BlossomSolver(const Graph& graph)
      : vertex_count_(graph.vertex_count()),
        unmatched_(vertex_count_),
        adjacency_(build_simple_adjacency(graph)),
        match_(vertex_count_, unmatched_),
        parent_(vertex_count_, unmatched_),
        base_(vertex_count_),
        used_(vertex_count_, false),
        blossom_(vertex_count_, false) {}

  [[nodiscard]] GeneralMatchingResult solve() {
    for (Vertex root = 0; root < vertex_count_; ++root) {
      if (match_[root] == unmatched_) {
        static_cast<void>(find_augmenting_path(root));
      }
    }

    std::size_t cardinality = 0;
    std::vector<std::optional<Vertex>> output(vertex_count_);
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      if (match_[vertex] != unmatched_) {
        output[vertex] = match_[vertex];
        if (vertex < match_[vertex]) {
          ++cardinality;
        }
      }
    }
    return GeneralMatchingResult{cardinality, std::move(output)};
  }

 private:
  [[nodiscard]] static std::vector<std::vector<Vertex>> build_simple_adjacency(
      const Graph& graph) {
    if (graph.directed()) {
      throw std::invalid_argument("general matching requires undirected input");
    }

    const std::size_t count = graph.vertex_count();
    std::vector<std::vector<Vertex>> adjacency(count);
    std::vector<std::vector<bool>> seen(
        count, std::vector<bool>(count, false));
    for (Vertex from = 0; from < count; ++from) {
      for (const auto& edge : graph.neighbors(from)) {
        if (edge.to == from || seen[from][edge.to]) {
          continue;
        }
        seen[from][edge.to] = true;
        adjacency[from].push_back(edge.to);
      }
    }
    return adjacency;
  }

  [[nodiscard]] Vertex lowest_common_base(Vertex first, Vertex second) const {
    std::vector<bool> first_path(vertex_count_, false);
    while (true) {
      first = base_[first];
      first_path[first] = true;
      if (match_[first] == unmatched_) {
        break;
      }
      first = parent_[match_[first]];
    }

    while (true) {
      second = base_[second];
      if (first_path[second]) {
        return second;
      }
      second = parent_[match_[second]];
    }
  }

  void mark_blossom_path(Vertex vertex, Vertex blossom_base, Vertex child) {
    while (base_[vertex] != blossom_base) {
      blossom_[base_[vertex]] = true;
      blossom_[base_[match_[vertex]]] = true;
      parent_[vertex] = child;
      child = match_[vertex];
      vertex = parent_[match_[vertex]];
    }
  }

  [[nodiscard]] bool find_augmenting_path(Vertex root) {
    std::fill(used_.begin(), used_.end(), false);
    std::fill(parent_.begin(), parent_.end(), unmatched_);
    std::iota(base_.begin(), base_.end(), Vertex{0});

    std::queue<Vertex> queue;
    queue.push(root);
    used_[root] = true;

    while (!queue.empty()) {
      const Vertex vertex = queue.front();
      queue.pop();
      for (const Vertex neighbor : adjacency_[vertex]) {
        if (base_[vertex] == base_[neighbor] ||
            match_[vertex] == neighbor) {
          continue;
        }

        const bool closes_blossom =
            neighbor == root ||
            (match_[neighbor] != unmatched_ &&
             parent_[match_[neighbor]] != unmatched_);
        if (closes_blossom) {
          const Vertex blossom_base = lowest_common_base(vertex, neighbor);
          std::fill(blossom_.begin(), blossom_.end(), false);
          mark_blossom_path(vertex, blossom_base, neighbor);
          mark_blossom_path(neighbor, blossom_base, vertex);
          for (Vertex candidate = 0; candidate < vertex_count_; ++candidate) {
            if (!blossom_[base_[candidate]]) {
              continue;
            }
            base_[candidate] = blossom_base;
            if (!used_[candidate]) {
              used_[candidate] = true;
              queue.push(candidate);
            }
          }
        } else if (parent_[neighbor] == unmatched_) {
          parent_[neighbor] = vertex;
          if (match_[neighbor] == unmatched_) {
            augment(neighbor);
            return true;
          }
          const Vertex matched = match_[neighbor];
          if (!used_[matched]) {
            used_[matched] = true;
            queue.push(matched);
          }
        }
      }
    }
    return false;
  }

  void augment(Vertex terminal) {
    Vertex vertex = terminal;
    while (vertex != unmatched_) {
      const Vertex previous = parent_[vertex];
      const Vertex next =
          previous == unmatched_ ? unmatched_ : match_[previous];
      match_[vertex] = previous;
      if (previous != unmatched_) {
        match_[previous] = vertex;
      }
      vertex = next;
    }
  }

  std::size_t vertex_count_;
  Vertex unmatched_;
  std::vector<std::vector<Vertex>> adjacency_;
  std::vector<Vertex> match_;
  std::vector<Vertex> parent_;
  std::vector<Vertex> base_;
  std::vector<bool> used_;
  std::vector<bool> blossom_;
};

}  // namespace

GeneralMatchingResult edmonds_blossom_maximum_matching(const Graph& graph) {
  return BlossomSolver(graph).solve();
}

}  // namespace algorithms::graphs
