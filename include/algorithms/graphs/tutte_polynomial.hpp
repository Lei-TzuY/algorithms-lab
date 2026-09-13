#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::graphs {

inline constexpr std::size_t kMaxExactTutteLogicalEdges = 20;

struct TutteTerm {
  std::size_t x_degree{};
  std::size_t y_degree{};
  std::uint64_t coefficient{};

  friend bool operator==(const TutteTerm&, const TutteTerm&) = default;
};

struct TuttePolynomialResult {
  std::vector<TutteTerm> terms;
  std::size_t logical_edge_count{};
  std::size_t evaluated_states{};

  friend bool operator==(const TuttePolynomialResult&,
                         const TuttePolynomialResult&) = default;
};

namespace detail {

using Endpoint = std::pair<std::size_t, std::size_t>;
using Polynomial = std::map<std::pair<std::size_t, std::size_t>, std::uint64_t>;

struct TutteState {
  std::size_t vertex_count{};
  std::vector<Endpoint> edges;

  friend bool operator<(const TutteState& lhs, const TutteState& rhs) {
    return std::tie(lhs.vertex_count, lhs.edges) <
           std::tie(rhs.vertex_count, rhs.edges);
  }
};

inline std::uint64_t checked_add_u64(std::uint64_t lhs, std::uint64_t rhs) {
  if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
    throw std::overflow_error("Tutte polynomial coefficient overflow");
  }
  return lhs + rhs;
}

inline void canonicalize_edges(std::vector<Endpoint>& edges) {
  for (auto& [first, second] : edges) {
    if (second < first) {
      std::swap(first, second);
    }
  }
  std::sort(edges.begin(), edges.end());
}

inline std::vector<Endpoint> logical_edges(const Graph& graph) {
  std::vector<Endpoint> edges;
  for (std::size_t from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from == edge.to || from < edge.to) {
        edges.emplace_back(from, edge.to);
      }
    }
  }
  canonicalize_edges(edges);
  return edges;
}

inline bool edge_is_bridge(const TutteState& state, std::size_t edge_index) {
  const auto [source, target] = state.edges.at(edge_index);
  if (source == target) {
    return false;
  }

  std::vector<std::vector<std::size_t>> adjacency(state.vertex_count);
  for (std::size_t index = 0; index < state.edges.size(); ++index) {
    if (index == edge_index) {
      continue;
    }
    const auto [first, second] = state.edges[index];
    if (first == second) {
      continue;
    }
    adjacency[first].push_back(second);
    adjacency[second].push_back(first);
  }

  std::vector<unsigned char> visited(state.vertex_count, 0);
  std::queue<std::size_t> pending;
  visited[source] = 1;
  pending.push(source);
  while (!pending.empty()) {
    const auto vertex = pending.front();
    pending.pop();
    for (const auto neighbor : adjacency[vertex]) {
      if (visited[neighbor] == 0) {
        visited[neighbor] = 1;
        pending.push(neighbor);
      }
    }
  }
  return visited[target] == 0;
}

inline TutteState delete_edge(const TutteState& state, std::size_t edge_index) {
  TutteState result = state;
  result.edges.erase(result.edges.begin() + static_cast<std::ptrdiff_t>(edge_index));
  return result;
}

inline TutteState contract_edge(const TutteState& state, std::size_t edge_index) {
  const auto [low, high] = state.edges.at(edge_index);
  if (low == high) {
    throw std::logic_error("cannot contract a loop in Tutte recurrence");
  }

  TutteState result;
  result.vertex_count = state.vertex_count - 1;
  result.edges.reserve(state.edges.size() - 1);

  const auto remap = [low, high](std::size_t vertex) {
    if (vertex == high) {
      return low;
    }
    if (vertex > high) {
      return vertex - 1;
    }
    return vertex;
  };

  for (std::size_t index = 0; index < state.edges.size(); ++index) {
    if (index == edge_index) {
      continue;
    }
    auto first = remap(state.edges[index].first);
    auto second = remap(state.edges[index].second);
    if (second < first) {
      std::swap(first, second);
    }
    result.edges.emplace_back(first, second);
  }
  canonicalize_edges(result.edges);
  return result;
}

inline Polynomial multiply_by_x(const Polynomial& input) {
  Polynomial output;
  for (const auto& [degree, coefficient] : input) {
    output.emplace(std::make_pair(degree.first + 1, degree.second), coefficient);
  }
  return output;
}

inline Polynomial multiply_by_y(const Polynomial& input) {
  Polynomial output;
  for (const auto& [degree, coefficient] : input) {
    output.emplace(std::make_pair(degree.first, degree.second + 1), coefficient);
  }
  return output;
}

inline Polynomial add_polynomials(const Polynomial& left, const Polynomial& right) {
  Polynomial result = left;
  for (const auto& [degree, coefficient] : right) {
    auto& slot = result[degree];
    slot = checked_add_u64(slot, coefficient);
  }
  return result;
}

class TutteSolver {
 public:
  [[nodiscard]] Polynomial solve(TutteState state) {
    canonicalize_edges(state.edges);
    if (const auto found = memo_.find(state); found != memo_.end()) {
      return found->second;
    }
    ++evaluated_states_;

    Polynomial result;
    if (state.edges.empty()) {
      result.emplace(std::make_pair(0U, 0U), 1U);
    } else {
      constexpr std::size_t chosen = 0;
      const auto [first, second] = state.edges[chosen];
      if (first == second) {
        result = multiply_by_y(solve(delete_edge(state, chosen)));
      } else if (edge_is_bridge(state, chosen)) {
        result = multiply_by_x(solve(contract_edge(state, chosen)));
      } else {
        result = add_polynomials(solve(delete_edge(state, chosen)),
                                 solve(contract_edge(state, chosen)));
      }
    }

    memo_.emplace(std::move(state), result);
    return result;
  }

  [[nodiscard]] std::size_t evaluated_states() const noexcept {
    return evaluated_states_;
  }

 private:
  std::map<TutteState, Polynomial> memo_;
  std::size_t evaluated_states_{};
};

}  // namespace detail

inline TuttePolynomialResult exact_tutte_polynomial(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("Tutte polynomial requires an undirected graph");
  }

  auto edges = detail::logical_edges(graph);
  const auto edge_count = edges.size();
  if (edge_count > kMaxExactTutteLogicalEdges) {
    throw std::length_error("Tutte polynomial edge limit exceeded");
  }

  detail::TutteSolver solver;
  const auto polynomial = solver.solve(
      detail::TutteState{graph.vertex_count(), std::move(edges)});

  TuttePolynomialResult result;
  result.logical_edge_count = edge_count;
  result.evaluated_states = solver.evaluated_states();
  result.terms.reserve(polynomial.size());
  for (const auto& [degree, coefficient] : polynomial) {
    if (coefficient != 0) {
      result.terms.push_back(TutteTerm{degree.first, degree.second, coefficient});
    }
  }
  return result;
}

inline std::uint64_t tutte_coefficient(const TuttePolynomialResult& polynomial,
                                       std::size_t x_degree,
                                       std::size_t y_degree) {
  for (const auto& term : polynomial.terms) {
    if (term.x_degree == x_degree && term.y_degree == y_degree) {
      return term.coefficient;
    }
  }
  return 0;
}

}  // namespace algorithms::graphs
