#include "algorithms/graphs/dominance_frontier.hpp"

#include <deque>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

DominanceFrontierIndex::DominanceFrontierIndex(const Graph& graph, Vertex start)
    : vertex_count_(graph.vertex_count()),
      start_(start),
      reachable_(vertex_count_, 0U),
      frontier_(vertex_count_) {
  const DominatorTree dominators(graph, start_);
  std::vector<std::vector<Vertex>> predecessors(vertex_count_);

  for (Vertex source = 0U; source < vertex_count_; ++source) {
    if (!dominators.reachable(source)) {
      continue;
    }
    reachable_[source] = 1U;
    for (const Edge& edge : graph.neighbors(source)) {
      if (dominators.reachable(edge.to)) {
        predecessors[edge.to].push_back(source);
      }
    }
  }

  for (Vertex dominator = 0U; dominator < vertex_count_; ++dominator) {
    if (reachable_[dominator] == 0U) {
      continue;
    }

    for (Vertex block = 0U; block < vertex_count_; ++block) {
      if (reachable_[block] == 0U) {
        continue;
      }

      const bool strictly_dominates =
          dominator != block && dominators.dominates(dominator, block);
      if (strictly_dominates) {
        continue;
      }

      bool dominates_predecessor = false;
      for (const Vertex predecessor : predecessors[block]) {
        if (dominators.dominates(dominator, predecessor)) {
          dominates_predecessor = true;
          break;
        }
      }
      if (dominates_predecessor) {
        frontier_[dominator].push_back(block);
      }
    }
  }
}

std::size_t DominanceFrontierIndex::vertex_count() const noexcept {
  return vertex_count_;
}

Vertex DominanceFrontierIndex::start() const noexcept { return start_; }

bool DominanceFrontierIndex::reachable(Vertex vertex) const {
  validate_vertex(vertex);
  return reachable_[vertex] != 0U;
}

const std::vector<Vertex>& DominanceFrontierIndex::frontier(Vertex vertex) const {
  validate_vertex(vertex);
  return frontier_[vertex];
}

const std::vector<std::vector<Vertex>>&
DominanceFrontierIndex::frontiers() const noexcept {
  return frontier_;
}

std::vector<Vertex> DominanceFrontierIndex::iterated_frontier(
    const std::vector<Vertex>& definitions) const {
  std::vector<unsigned char> is_definition(vertex_count_, 0U);
  std::vector<unsigned char> in_result(vertex_count_, 0U);
  std::vector<unsigned char> queued(vertex_count_, 0U);
  std::deque<Vertex> worklist;

  for (const Vertex vertex : definitions) {
    validate_vertex(vertex);
    if (reachable_[vertex] == 0U) {
      throw std::invalid_argument(
          "IDF definition block is unreachable from the dominance start");
    }
    if (is_definition[vertex] == 0U) {
      is_definition[vertex] = 1U;
      queued[vertex] = 1U;
      worklist.push_back(vertex);
    }
  }

  while (!worklist.empty()) {
    const Vertex current = worklist.front();
    worklist.pop_front();

    for (const Vertex block : frontier_[current]) {
      if (in_result[block] != 0U) {
        continue;
      }
      in_result[block] = 1U;
      if (is_definition[block] == 0U && queued[block] == 0U) {
        queued[block] = 1U;
        worklist.push_back(block);
      }
    }
  }

  std::vector<Vertex> result;
  for (Vertex vertex = 0U; vertex < vertex_count_; ++vertex) {
    if (in_result[vertex] != 0U) {
      result.push_back(vertex);
    }
  }
  return result;
}

void DominanceFrontierIndex::validate_vertex(Vertex vertex) const {
  if (vertex >= vertex_count_) {
    throw std::out_of_range("dominance-frontier vertex out of range");
  }
}

}  // namespace algorithms::graphs
