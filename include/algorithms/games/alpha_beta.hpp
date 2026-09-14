#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::games {

struct GameTreeNode {
  std::vector<std::size_t> children;
  std::optional<std::int64_t> utility;
};

struct AlphaBetaResult {
  std::int64_t value{};
  std::vector<std::size_t> principal_variation;
  std::size_t visited_nodes{};
  std::size_t evaluated_leaves{};
  std::size_t cutoffs{};
};

namespace alpha_beta_detail {

inline void validate_tree(const std::vector<GameTreeNode>& nodes,
                          std::size_t root) {
  if (nodes.empty()) {
    throw std::invalid_argument("alpha-beta game tree must be non-empty");
  }
  if (root >= nodes.size()) {
    throw std::out_of_range("alpha-beta root is out of range");
  }

  std::vector<std::size_t> indegree(nodes.size(), 0U);
  for (std::size_t vertex = 0U; vertex < nodes.size(); ++vertex) {
    const auto& node = nodes[vertex];
    if (node.children.empty()) {
      if (!node.utility.has_value()) {
        throw std::invalid_argument("alpha-beta leaf must carry a utility");
      }
    } else if (node.utility.has_value()) {
      throw std::invalid_argument(
          "alpha-beta internal node must not carry a utility");
    }

    for (const std::size_t child : node.children) {
      if (child >= nodes.size()) {
        throw std::out_of_range("alpha-beta child is out of range");
      }
      ++indegree[child];
    }
  }

  if (indegree[root] != 0U) {
    throw std::invalid_argument("alpha-beta root must have indegree zero");
  }
  for (std::size_t vertex = 0U; vertex < nodes.size(); ++vertex) {
    if (vertex != root && indegree[vertex] != 1U) {
      throw std::invalid_argument(
          "alpha-beta input must be one rooted tree");
    }
  }

  std::vector<unsigned char> seen(nodes.size(), 0U);
  std::vector<std::size_t> stack{root};
  std::size_t reached = 0U;
  while (!stack.empty()) {
    const std::size_t vertex = stack.back();
    stack.pop_back();
    if (seen[vertex] != 0U) {
      throw std::invalid_argument("alpha-beta input contains a cycle");
    }
    seen[vertex] = 1U;
    ++reached;
    for (const std::size_t child : nodes[vertex].children) {
      stack.push_back(child);
    }
  }
  if (reached != nodes.size()) {
    throw std::invalid_argument(
        "alpha-beta input must be fully reachable from root");
  }
}

struct SearchState {
  const std::vector<GameTreeNode>& nodes;
  std::vector<std::optional<std::size_t>> choice;
  std::size_t visited_nodes{};
  std::size_t evaluated_leaves{};
  std::size_t cutoffs{};
};

inline std::int64_t search(SearchState& state, std::size_t vertex,
                           bool maximizing,
                           std::optional<std::int64_t> alpha,
                           std::optional<std::int64_t> beta) {
  ++state.visited_nodes;
  const auto& node = state.nodes[vertex];
  if (node.children.empty()) {
    ++state.evaluated_leaves;
    return *node.utility;
  }

  std::optional<std::int64_t> best;
  std::optional<std::size_t> best_child;

  for (const std::size_t child : node.children) {
    const std::int64_t value =
        search(state, child, !maximizing, alpha, beta);
    if (!best.has_value() || (maximizing ? value > *best : value < *best)) {
      best = value;
      best_child = child;
    }

    if (maximizing) {
      if (!alpha.has_value() || *best > *alpha) {
        alpha = *best;
      }
    } else if (!beta.has_value() || *best < *beta) {
      beta = *best;
    }

    if (alpha.has_value() && beta.has_value() && *alpha >= *beta) {
      ++state.cutoffs;
      break;
    }
  }

  state.choice[vertex] = *best_child;
  return *best;
}

}  // namespace alpha_beta_detail

inline AlphaBetaResult alpha_beta_minimax(
    const std::vector<GameTreeNode>& nodes, std::size_t root = 0U,
    bool maximizing_root = true) {
  alpha_beta_detail::validate_tree(nodes, root);

  alpha_beta_detail::SearchState state{nodes,
                                       std::vector<std::optional<std::size_t>>(
                                           nodes.size())};
  AlphaBetaResult result;
  result.value = alpha_beta_detail::search(state, root, maximizing_root,
                                            std::nullopt, std::nullopt);
  result.visited_nodes = state.visited_nodes;
  result.evaluated_leaves = state.evaluated_leaves;
  result.cutoffs = state.cutoffs;

  std::size_t vertex = root;
  result.principal_variation.push_back(vertex);
  while (!nodes[vertex].children.empty()) {
    if (!state.choice[vertex].has_value()) {
      throw std::logic_error("alpha-beta principal variation is incomplete");
    }
    vertex = *state.choice[vertex];
    result.principal_variation.push_back(vertex);
  }
  return result;
}

}  // namespace algorithms::games
