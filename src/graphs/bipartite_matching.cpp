#include "algorithms/graphs/bipartite_matching.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
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

constexpr std::size_t kNoColor = std::numeric_limits<std::size_t>::max();

struct LogicalEdge {
  Vertex first;
  Vertex second;
  Weight weight;
  std::size_t left;
  std::size_t right;
};

struct PairBucket {
  std::size_t active_count{};
  std::vector<std::size_t> original_edge_ids;
  std::size_t original_cursor{};
  std::size_t dummy_count{};
};

[[nodiscard]] std::vector<int> compute_bipartition(const Graph& graph) {
  const std::size_t vertex_count = graph.vertex_count();
  std::vector<int> side(vertex_count, -1);
  std::queue<Vertex> queue;

  for (Vertex start = 0; start < vertex_count; ++start) {
    if (side[start] != -1) {
      continue;
    }
    side[start] = 0;
    queue.push(start);

    while (!queue.empty()) {
      const Vertex vertex = queue.front();
      queue.pop();
      for (const auto& edge : graph.neighbors(vertex)) {
        if (edge.to == vertex) {
          throw std::invalid_argument(
              "bipartite edge coloring does not allow self-loops");
        }
        if (side[edge.to] == -1) {
          side[edge.to] = 1 - side[vertex];
          queue.push(edge.to);
        } else if (side[edge.to] == side[vertex]) {
          throw std::invalid_argument(
              "bipartite edge coloring requires a bipartite graph");
        }
      }
    }
  }
  return side;
}

[[nodiscard]] std::vector<LogicalEdge> enumerate_logical_edges(
    const Graph& graph, const std::vector<int>& side,
    const std::vector<std::size_t>& side_index) {
  std::vector<LogicalEdge> edges;
  for (Vertex first = 0; first < graph.vertex_count(); ++first) {
    for (const auto& edge : graph.neighbors(first)) {
      if (first >= edge.to) {
        continue;
      }
      if (side[first] == 0) {
        edges.push_back(
            LogicalEdge{first, edge.to, edge.weight, side_index[first],
                        side_index[edge.to]});
      } else {
        edges.push_back(
            LogicalEdge{first, edge.to, edge.weight, side_index[edge.to],
                        side_index[first]});
      }
    }
  }
  return edges;
}

}  // namespace

BipartiteMatchingResult hopcroft_karp(std::size_t left_count,
                                      std::size_t right_count,
                                      std::span<const BipartiteEdge> edges) {
  return HopcroftKarpSolver(left_count, right_count, edges).solve();
}

BipartiteEdgeColoringResult minimum_bipartite_edge_coloring(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "bipartite edge coloring requires an undirected graph");
  }

  const std::vector<int> side = compute_bipartition(graph);
  std::vector<std::size_t> side_index(graph.vertex_count(), 0U);
  std::size_t left_count = 0;
  std::size_t right_count = 0;
  std::vector<bool> left_partition(graph.vertex_count(), false);
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (side[vertex] == 0) {
      side_index[vertex] = left_count++;
      left_partition[vertex] = true;
    } else {
      side_index[vertex] = right_count++;
    }
  }

  const auto logical_edges = enumerate_logical_edges(graph, side, side_index);
  std::vector<std::size_t> degree(graph.vertex_count(), 0U);
  std::size_t maximum_degree = 0;
  for (const auto& edge : logical_edges) {
    ++degree[edge.first];
    ++degree[edge.second];
    maximum_degree = std::max({maximum_degree, degree[edge.first],
                               degree[edge.second]});
  }

  std::vector<ColoredUndirectedEdge> output_edges;
  output_edges.reserve(logical_edges.size());
  for (std::size_t edge_id = 0; edge_id < logical_edges.size(); ++edge_id) {
    const auto& edge = logical_edges[edge_id];
    output_edges.push_back(ColoredUndirectedEdge{
        edge_id, edge.first, edge.second, edge.weight, kNoColor});
  }

  if (maximum_degree == 0) {
    return BipartiteEdgeColoringResult{0U, std::move(left_partition),
                                       std::move(output_edges)};
  }

  const std::size_t balanced_count = std::max(left_count, right_count);
  std::vector<std::size_t> left_degree(balanced_count, 0U);
  std::vector<std::size_t> right_degree(balanced_count, 0U);
  std::map<std::pair<std::size_t, std::size_t>, PairBucket> buckets;

  for (std::size_t edge_id = 0; edge_id < logical_edges.size(); ++edge_id) {
    const auto& edge = logical_edges[edge_id];
    ++left_degree[edge.left];
    ++right_degree[edge.right];
    auto& bucket = buckets[{edge.left, edge.right}];
    ++bucket.active_count;
    bucket.original_edge_ids.push_back(edge_id);
  }

  std::vector<std::size_t> left_deficit(balanced_count, maximum_degree);
  std::vector<std::size_t> right_deficit(balanced_count, maximum_degree);
  for (std::size_t index = 0; index < balanced_count; ++index) {
    left_deficit[index] -= left_degree[index];
    right_deficit[index] -= right_degree[index];
  }

  std::size_t left = 0;
  std::size_t right = 0;
  while (left < balanced_count && right < balanced_count) {
    while (left < balanced_count && left_deficit[left] == 0U) {
      ++left;
    }
    while (right < balanced_count && right_deficit[right] == 0U) {
      ++right;
    }
    if (left == balanced_count || right == balanced_count) {
      break;
    }
    const std::size_t copies =
        std::min(left_deficit[left], right_deficit[right]);
    auto& bucket = buckets[{left, right}];
    bucket.active_count += copies;
    bucket.dummy_count += copies;
    left_deficit[left] -= copies;
    right_deficit[right] -= copies;
  }

  if (!std::all_of(left_deficit.begin(), left_deficit.end(),
                   [](std::size_t value) { return value == 0U; }) ||
      !std::all_of(right_deficit.begin(), right_deficit.end(),
                   [](std::size_t value) { return value == 0U; })) {
    throw std::logic_error("failed to regularize bipartite multigraph");
  }

  for (std::size_t color = 0; color < maximum_degree; ++color) {
    std::vector<BipartiteEdge> support;
    support.reserve(buckets.size());
    for (const auto& [key, bucket] : buckets) {
      if (bucket.active_count != 0U) {
        support.push_back(BipartiteEdge{key.first, key.second});
      }
    }

    const auto matching =
        hopcroft_karp(balanced_count, balanced_count, support);
    if (matching.cardinality != balanced_count) {
      throw std::logic_error(
          "regular bipartite multigraph lacked a perfect matching");
    }

    for (std::size_t left_vertex = 0; left_vertex < balanced_count;
         ++left_vertex) {
      if (!matching.left_match[left_vertex].has_value()) {
        throw std::logic_error("perfect matching witness was incomplete");
      }
      const std::size_t right_vertex = *matching.left_match[left_vertex];
      auto found = buckets.find({left_vertex, right_vertex});
      if (found == buckets.end() || found->second.active_count == 0U) {
        throw std::logic_error("matching referenced an inactive edge bucket");
      }
      auto& bucket = found->second;
      --bucket.active_count;
      if (bucket.original_cursor < bucket.original_edge_ids.size()) {
        const std::size_t edge_id =
            bucket.original_edge_ids[bucket.original_cursor++];
        output_edges[edge_id].color = color;
      } else {
        if (bucket.dummy_count == 0U) {
          throw std::logic_error("edge bucket accounting underflow");
        }
        --bucket.dummy_count;
      }
    }
  }

  for (const auto& edge : output_edges) {
    if (edge.color == kNoColor) {
      throw std::logic_error("an original edge was left uncolored");
    }
  }
  for (const auto& [key, bucket] : buckets) {
    static_cast<void>(key);
    if (bucket.active_count != 0U || bucket.dummy_count != 0U ||
        bucket.original_cursor != bucket.original_edge_ids.size()) {
      throw std::logic_error("edge bucket remained after all color classes");
    }
  }

  return BipartiteEdgeColoringResult{maximum_degree,
                                     std::move(left_partition),
                                     std::move(output_edges)};
}

}  // namespace algorithms::graphs
