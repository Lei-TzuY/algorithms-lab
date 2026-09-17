#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

class FunctionalGraphIndex {
 public:
  explicit FunctionalGraphIndex(std::vector<Vertex> successor)
      : successor_(std::move(successor)),
        component_(successor_.size(), 0U),
        cycle_entry_(successor_.size(), 0U),
        distance_to_cycle_(successor_.size(), 0U),
        entry_cycle_position_(successor_.size(), 0U) {
    const std::size_t n = successor_.size();
    for (const Vertex to : successor_) {
      if (to >= n) {
        throw std::out_of_range("functional-graph successor out of range");
      }
    }

    if (n == 0U) {
      return;
    }

    std::vector<std::size_t> indegree(n, 0U);
    std::vector<std::vector<Vertex>> reverse(n);
    for (Vertex vertex = 0U; vertex < n; ++vertex) {
      ++indegree[successor_[vertex]];
      reverse[successor_[vertex]].push_back(vertex);
    }

    std::queue<Vertex> peel;
    for (Vertex vertex = 0U; vertex < n; ++vertex) {
      if (indegree[vertex] == 0U) {
        peel.push(vertex);
      }
    }
    while (!peel.empty()) {
      const Vertex vertex = peel.front();
      peel.pop();
      const Vertex to = successor_[vertex];
      --indegree[to];
      if (indegree[to] == 0U) {
        peel.push(to);
      }
    }

    std::vector<std::uint8_t> assigned_cycle(n, 0U);
    for (Vertex start = 0U; start < n; ++start) {
      if (indegree[start] == 0U || assigned_cycle[start] != 0U) {
        continue;
      }

      std::vector<Vertex> cycle;
      Vertex current = start;
      do {
        cycle.push_back(current);
        assigned_cycle[current] = 1U;
        current = successor_[current];
      } while (current != start);

      // start is the smallest vertex on this still-unassigned cycle because the
      // outer scan is ascending, so the successor order is already canonical.
      const std::size_t component_id = cycles_.size();
      for (std::size_t position = 0U; position < cycle.size(); ++position) {
        const Vertex vertex = cycle[position];
        component_[vertex] = component_id;
        cycle_entry_[vertex] = vertex;
        distance_to_cycle_[vertex] = 0U;
        entry_cycle_position_[vertex] = position;
      }
      cycles_.push_back(std::move(cycle));
    }

    std::queue<Vertex> outward;
    for (const auto& cycle : cycles_) {
      for (const Vertex vertex : cycle) {
        outward.push(vertex);
      }
    }
    std::vector<std::uint8_t> assigned(n, 0U);
    for (const auto& cycle : cycles_) {
      for (const Vertex vertex : cycle) {
        assigned[vertex] = 1U;
      }
    }
    while (!outward.empty()) {
      const Vertex parent = outward.front();
      outward.pop();
      for (const Vertex child : reverse[parent]) {
        if (assigned[child] != 0U) {
          continue;
        }
        assigned[child] = 1U;
        component_[child] = component_[parent];
        cycle_entry_[child] = cycle_entry_[parent];
        distance_to_cycle_[child] = distance_to_cycle_[parent] + 1U;
        entry_cycle_position_[child] = entry_cycle_position_[parent];
        outward.push(child);
      }
    }

    constexpr std::size_t kLevels = std::numeric_limits<std::uint64_t>::digits;
    jump_.resize(kLevels, std::vector<Vertex>(n, 0U));
    jump_[0] = successor_;
    for (std::size_t level = 1U; level < kLevels; ++level) {
      for (Vertex vertex = 0U; vertex < n; ++vertex) {
        jump_[level][vertex] = jump_[level - 1U][jump_[level - 1U][vertex]];
      }
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return successor_.size(); }
  [[nodiscard]] std::size_t component_count() const noexcept {
    return cycles_.size();
  }
  [[nodiscard]] const std::vector<std::vector<Vertex>>& cycles() const noexcept {
    return cycles_;
  }

  [[nodiscard]] Vertex successor(const Vertex vertex) const {
    validate_vertex(vertex);
    return successor_[vertex];
  }
  [[nodiscard]] std::size_t component(const Vertex vertex) const {
    validate_vertex(vertex);
    return component_[vertex];
  }
  [[nodiscard]] Vertex cycle_entry(const Vertex vertex) const {
    validate_vertex(vertex);
    return cycle_entry_[vertex];
  }
  [[nodiscard]] std::size_t distance_to_cycle(const Vertex vertex) const {
    validate_vertex(vertex);
    return distance_to_cycle_[vertex];
  }
  [[nodiscard]] std::size_t cycle_length(const Vertex vertex) const {
    validate_vertex(vertex);
    return cycles_[component_[vertex]].size();
  }
  [[nodiscard]] std::size_t cycle_entry_position(
      const Vertex vertex) const {
    validate_vertex(vertex);
    return entry_cycle_position_[vertex];
  }

  [[nodiscard]] Vertex kth_successor(Vertex vertex,
                                       std::uint64_t steps) const {
    validate_vertex(vertex);
    std::size_t bit = 0U;
    while (steps != 0U) {
      if ((steps & 1ULL) != 0ULL) {
        vertex = jump_[bit][vertex];
      }
      steps >>= 1U;
      ++bit;
    }
    return vertex;
  }

  [[nodiscard]] std::optional<std::uint64_t> steps_to_reach(
      const Vertex from, const Vertex to) const {
    validate_vertex(from);
    validate_vertex(to);
    if (component_[from] != component_[to]) {
      return std::nullopt;
    }

    if (distance_to_cycle_[to] != 0U) {
      if (distance_to_cycle_[from] < distance_to_cycle_[to]) {
        return std::nullopt;
      }
      const std::size_t difference =
          distance_to_cycle_[from] - distance_to_cycle_[to];
      if (kth_successor(from, static_cast<std::uint64_t>(difference)) != to) {
        return std::nullopt;
      }
      return static_cast<std::uint64_t>(difference);
    }

    const std::size_t component_id = component_[from];
    const std::size_t length = cycles_[component_id].size();
    const std::size_t entry_position = entry_cycle_position_[from];
    const std::size_t target_position = entry_cycle_position_[to];
    const std::size_t cycle_steps =
        (target_position + length - entry_position) % length;
    const std::uint64_t prefix =
        static_cast<std::uint64_t>(distance_to_cycle_[from]);
    return prefix + static_cast<std::uint64_t>(cycle_steps);
  }

 private:
  void validate_vertex(const Vertex vertex) const {
    if (vertex >= successor_.size()) {
      throw std::out_of_range("functional-graph vertex out of range");
    }
  }

  std::vector<Vertex> successor_;
  std::vector<std::size_t> component_;
  std::vector<Vertex> cycle_entry_;
  std::vector<std::size_t> distance_to_cycle_;
  std::vector<std::size_t> entry_cycle_position_;
  std::vector<std::vector<Vertex>> cycles_;
  std::vector<std::vector<Vertex>> jump_;
};

}  // namespace algorithms::graphs
