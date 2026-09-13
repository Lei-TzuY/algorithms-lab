#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <deque>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::games {

struct SpragueGrundyResult {
  std::vector<std::size_t> grundy;
  std::vector<std::optional<algorithms::graphs::Vertex>> winning_move;
  std::size_t max_grundy = 0U;
};

[[nodiscard]] inline std::size_t sprague_grundy_nim_sum(
    std::span<const std::size_t> component_grundy) noexcept {
  std::size_t result = 0U;
  for (const std::size_t value : component_grundy) {
    result ^= value;
  }
  return result;
}

[[nodiscard]] inline SpragueGrundyResult analyze_impartial_dag(
    const algorithms::graphs::Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument("Sprague-Grundy analysis requires a directed game graph");
  }

  const std::size_t n = graph.vertex_count();
  std::vector<std::size_t> indegree(n, 0U);
  for (std::size_t vertex = 0U; vertex < n; ++vertex) {
    for (const auto& edge : graph.neighbors(vertex)) {
      ++indegree[edge.to];
    }
  }

  std::deque<algorithms::graphs::Vertex> ready;
  for (std::size_t vertex = 0U; vertex < n; ++vertex) {
    if (indegree[vertex] == 0U) {
      ready.push_back(vertex);
    }
  }

  std::vector<algorithms::graphs::Vertex> topological_order;
  topological_order.reserve(n);
  while (!ready.empty()) {
    const auto vertex = ready.front();
    ready.pop_front();
    topological_order.push_back(vertex);
    for (const auto& edge : graph.neighbors(vertex)) {
      if (indegree[edge.to] == 0U) {
        throw std::logic_error("Sprague-Grundy indegree accounting underflow");
      }
      --indegree[edge.to];
      if (indegree[edge.to] == 0U) {
        ready.push_back(edge.to);
      }
    }
  }
  if (topological_order.size() != n) {
    throw std::invalid_argument("Sprague-Grundy analysis requires an acyclic game graph");
  }

  SpragueGrundyResult result;
  result.grundy.assign(n, 0U);
  result.winning_move.assign(n, std::nullopt);

  for (auto order_it = topological_order.rbegin();
       order_it != topological_order.rend(); ++order_it) {
    const auto vertex = *order_it;
    const auto& moves = graph.neighbors(vertex);
    std::vector<unsigned char> seen(moves.size() + 1U, 0U);
    for (const auto& edge : moves) {
      const std::size_t successor_grundy = result.grundy[edge.to];
      if (successor_grundy < seen.size()) {
        seen[successor_grundy] = 1U;
      }
    }

    std::size_t mex = 0U;
    while (mex < seen.size() && seen[mex] != 0U) {
      ++mex;
    }
    result.grundy[vertex] = mex;
    if (mex > result.max_grundy) {
      result.max_grundy = mex;
    }

    if (mex != 0U) {
      for (const auto& edge : moves) {
        if (result.grundy[edge.to] == 0U) {
          result.winning_move[vertex] = edge.to;
          break;
        }
      }
      if (!result.winning_move[vertex].has_value()) {
        throw std::logic_error("positive Grundy position has no move to zero");
      }
    }
  }
  return result;
}

}  // namespace algorithms::games
