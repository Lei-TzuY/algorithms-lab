#include "algorithms/constraints/two_sat.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/strongly_connected_components.hpp"
#include "algorithms/graphs/traversal.hpp"

namespace algorithms::constraints {
namespace {

using algorithms::graphs::Graph;
using algorithms::graphs::StronglyConnectedComponents;
using algorithms::graphs::Vertex;

Vertex literal_vertex(const TwoSatLiteral literal) {
  return literal.variable * 2U + (literal.positive ? 1U : 0U);
}

TwoSatLiteral vertex_literal(const Vertex vertex) {
  return TwoSatLiteral{vertex / 2U, (vertex % 2U) != 0U};
}

Vertex negated_vertex(const Vertex vertex) { return vertex ^ 1U; }

void validate_clauses(const std::size_t variable_count,
                      const std::span<const TwoSatClause> clauses) {
  if (variable_count > std::numeric_limits<std::size_t>::max() / 2U) {
    throw std::length_error("2-SAT literal graph size overflows size_t");
  }
  if (clauses.size() > std::numeric_limits<std::size_t>::max() / 2U) {
    throw std::length_error("2-SAT implication edge count overflows size_t");
  }
  for (const TwoSatClause& clause : clauses) {
    if (clause.first.variable >= variable_count ||
        clause.second.variable >= variable_count) {
      throw std::out_of_range("2-SAT literal variable out of range");
    }
  }
}

Graph build_implication_graph(const std::size_t variable_count,
                              const std::span<const TwoSatClause> clauses) {
  std::vector<std::pair<Vertex, Vertex>> implications;
  implications.reserve(clauses.size() * 2U);
  for (const TwoSatClause& clause : clauses) {
    const Vertex first = literal_vertex(clause.first);
    const Vertex second = literal_vertex(clause.second);
    implications.emplace_back(negated_vertex(first), second);
    implications.emplace_back(negated_vertex(second), first);
  }
  std::sort(implications.begin(), implications.end());
  implications.erase(std::unique(implications.begin(), implications.end()),
                     implications.end());

  Graph graph(variable_count * 2U, true);
  for (const auto& [from, to] : implications) {
    graph.add_edge(from, to);
  }
  return graph;
}

std::vector<TwoSatLiteral> implication_path(
    const Graph& graph, const StronglyConnectedComponents& decomposition,
    const Vertex start, const Vertex target) {
  const std::size_t component = decomposition.component_of[start];
  std::vector<Vertex> parent(graph.vertex_count(), graph.vertex_count());
  std::vector<bool> visited(graph.vertex_count(), false);
  std::deque<Vertex> queue;
  visited[start] = true;
  queue.push_back(start);

  while (!queue.empty() && !visited[target]) {
    const Vertex current = queue.front();
    queue.pop_front();
    for (const auto& edge : graph.neighbors(current)) {
      if (decomposition.component_of[edge.to] != component ||
          visited[edge.to]) {
        continue;
      }
      visited[edge.to] = true;
      parent[edge.to] = current;
      queue.push_back(edge.to);
    }
  }

  if (!visited[target]) {
    throw std::logic_error("SCC decomposition lacks a mutual-reachability path");
  }

  std::vector<TwoSatLiteral> reversed;
  for (Vertex current = target;; current = parent[current]) {
    reversed.push_back(vertex_literal(current));
    if (current == start) {
      break;
    }
  }
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

}  // namespace

TwoSatResult solve_two_sat(const std::size_t variable_count,
                           const std::span<const TwoSatClause> clauses) {
  validate_clauses(variable_count, clauses);
  const Graph implication_graph = build_implication_graph(variable_count, clauses);
  const StronglyConnectedComponents decomposition =
      algorithms::graphs::tarjan_strongly_connected_components(implication_graph);

  for (std::size_t variable = 0; variable < variable_count; ++variable) {
    const Vertex negative = variable * 2U;
    const Vertex positive = negative + 1U;
    if (decomposition.component_of[negative] !=
        decomposition.component_of[positive]) {
      continue;
    }
    return TwoSatResult{
        false,
        {},
        TwoSatContradictionWitness{
            variable,
            implication_path(implication_graph, decomposition, negative, positive),
            implication_path(implication_graph, decomposition, positive, negative)}};
  }

  const Graph condensation =
      algorithms::graphs::condensation_graph(implication_graph, decomposition);
  const auto topological = algorithms::graphs::topological_sort(condensation);
  if (!topological.has_value()) {
    throw std::logic_error("SCC condensation graph is cyclic");
  }
  std::vector<std::size_t> topological_rank(condensation.vertex_count());
  for (std::size_t index = 0; index < topological->size(); ++index) {
    topological_rank[(*topological)[index]] = index;
  }

  std::vector<bool> assignment(variable_count, false);
  for (std::size_t variable = 0; variable < variable_count; ++variable) {
    const Vertex negative = variable * 2U;
    const Vertex positive = negative + 1U;
    assignment[variable] =
        topological_rank[decomposition.component_of[positive]] >
        topological_rank[decomposition.component_of[negative]];
  }
  return TwoSatResult{true, std::move(assignment), std::nullopt};
}

}  // namespace algorithms::constraints
