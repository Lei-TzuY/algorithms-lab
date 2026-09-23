#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct CoffmanGrahamTwoProcessorSchedule {
  // Unique labels in [1,n], indexed by task.
  std::vector<std::size_t> labels;

  // Unit-time slots. processor[0] is filled before processor[1].
  std::vector<std::array<std::optional<std::size_t>, 2U>> slots;

  [[nodiscard]] std::size_t makespan() const noexcept {
    return slots.size();
  }

  friend bool operator==(const CoffmanGrahamTwoProcessorSchedule&,
                         const CoffmanGrahamTwoProcessorSchedule&) = default;
};

// Exact Coffman-Graham scheduling for unit-time precedence-constrained tasks on
// two identical processors.
//
// successors[u] contains every task v constrained by u -> v.
//
// The labeling convention is:
//   * labels are assigned 1,2,... from sinks toward sources;
//   * eligible unlabeled tasks have all successors already labeled;
//   * each eligible task's signature is its successor labels sorted decreasing;
//   * the lexicographically smallest signature wins;
//   * equal signatures break by smaller task index.
//
// Scheduling is the corresponding descending-label list schedule: at each unit
// slot, up to two currently ready tasks with greatest labels are executed.
//
// Throws std::invalid_argument for an out-of-range edge, duplicate edge, or
// directed cycle.
[[nodiscard]] inline CoffmanGrahamTwoProcessorSchedule
coffman_graham_two_processor_schedule(
    const std::vector<std::vector<std::size_t>>& successors) {
  const std::size_t n = successors.size();

  CoffmanGrahamTwoProcessorSchedule result;
  result.labels.assign(n, 0U);
  if (n == 0U) {
    return result;
  }

  std::vector<std::vector<std::size_t>> predecessors(n);
  for (std::size_t task = 0U; task < n; ++task) {
    std::vector<std::size_t> sorted_successors = successors[task];
    for (const std::size_t successor : sorted_successors) {
      if (successor >= n) {
        throw std::invalid_argument(
            "Coffman-Graham successor index out of range");
      }
    }
    std::sort(sorted_successors.begin(), sorted_successors.end());
    if (std::adjacent_find(sorted_successors.begin(),
                           sorted_successors.end()) !=
        sorted_successors.end()) {
      throw std::invalid_argument(
          "Coffman-Graham duplicate precedence edge");
    }

    for (const std::size_t successor : sorted_successors) {
      predecessors[successor].push_back(task);
    }
  }

  std::vector<std::size_t> remaining_successors(n, 0U);
  std::vector<std::vector<std::size_t>> signatures(n);
  std::vector<std::size_t> ready;
  ready.reserve(n);

  for (std::size_t task = 0U; task < n; ++task) {
    remaining_successors[task] = successors[task].size();
    if (remaining_successors[task] == 0U) {
      ready.push_back(task);
    }
  }

  const auto signature_less =
      [&](const std::size_t left, const std::size_t right) {
        const auto& a = signatures[left];
        const auto& b = signatures[right];
        if (std::lexicographical_compare(
                a.begin(), a.end(), b.begin(), b.end())) {
          return true;
        }
        if (std::lexicographical_compare(
                b.begin(), b.end(), a.begin(), a.end())) {
          return false;
        }
        return left < right;
      };

  for (std::size_t label = 1U; label <= n; ++label) {
    if (ready.empty()) {
      throw std::invalid_argument(
          "Coffman-Graham precedence graph contains a directed cycle");
    }

    std::size_t best_position = 0U;
    for (std::size_t position = 1U;
         position < ready.size(); ++position) {
      if (signature_less(ready[position], ready[best_position])) {
        best_position = position;
      }
    }

    const std::size_t task = ready[best_position];
    ready[best_position] = ready.back();
    ready.pop_back();
    result.labels[task] = label;

    for (const std::size_t predecessor : predecessors[task]) {
      if (remaining_successors[predecessor] == 0U) {
        throw std::logic_error(
            "Coffman-Graham successor accounting underflow");
      }
      --remaining_successors[predecessor];
      if (remaining_successors[predecessor] != 0U) {
        continue;
      }

      auto& signature = signatures[predecessor];
      signature.reserve(successors[predecessor].size());
      for (const std::size_t successor : successors[predecessor]) {
        const std::size_t successor_label = result.labels[successor];
        if (successor_label == 0U) {
          throw std::logic_error(
              "Coffman-Graham eligible task has unlabeled successor");
        }
        signature.push_back(successor_label);
      }
      std::sort(signature.begin(), signature.end(),
                std::greater<std::size_t>());
      ready.push_back(predecessor);
    }
  }

  // List scheduling in descending label order.
  std::vector<std::size_t> unsatisfied_predecessors(n, 0U);
  ready.clear();
  for (std::size_t task = 0U; task < n; ++task) {
    unsatisfied_predecessors[task] = predecessors[task].size();
    if (unsatisfied_predecessors[task] == 0U) {
      ready.push_back(task);
    }
  }

  std::size_t scheduled_count = 0U;
  while (scheduled_count < n) {
    if (ready.empty()) {
      throw std::logic_error(
          "Coffman-Graham scheduling lost all ready tasks");
    }

    std::array<std::optional<std::size_t>, 2U> slot{
        std::nullopt, std::nullopt};

    for (std::size_t processor = 0U;
         processor < 2U && !ready.empty(); ++processor) {
      std::size_t best_position = 0U;
      for (std::size_t position = 1U;
           position < ready.size(); ++position) {
        const std::size_t candidate = ready[position];
        const std::size_t incumbent = ready[best_position];
        if (result.labels[candidate] > result.labels[incumbent]) {
          best_position = position;
        }
      }

      const std::size_t task = ready[best_position];
      ready[best_position] = ready.back();
      ready.pop_back();
      slot[processor] = task;
    }

    // Tasks becoming ready here may run only in the next unit slot.
    for (const auto task : slot) {
      if (!task.has_value()) {
        continue;
      }
      ++scheduled_count;
      for (const std::size_t successor : successors[*task]) {
        if (unsatisfied_predecessors[successor] == 0U) {
          throw std::logic_error(
              "Coffman-Graham predecessor accounting underflow");
        }
        --unsatisfied_predecessors[successor];
        if (unsatisfied_predecessors[successor] == 0U) {
          ready.push_back(successor);
        }
      }
    }

    result.slots.push_back(slot);
  }

  return result;
}

}  // namespace algorithms::graphs
