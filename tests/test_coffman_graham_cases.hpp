#pragma once

#include "algorithms/graphs/coffman_graham.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::graphs::CoffmanGrahamTwoProcessorSchedule;
using algorithms::graphs::coffman_graham_two_processor_schedule;

namespace coffman_graham_test_detail {

inline bool valid_schedule(
    const std::vector<std::vector<std::size_t>>& successors,
    const CoffmanGrahamTwoProcessorSchedule& schedule) {
  const std::size_t n = successors.size();
  if (schedule.labels.size() != n) {
    return false;
  }

  std::vector<std::size_t> seen_label(n + 1U, 0U);
  for (std::size_t task = 0U; task < n; ++task) {
    const std::size_t label = schedule.labels[task];
    if (label == 0U || label > n || seen_label[label] != 0U) {
      return false;
    }
    seen_label[label] = 1U;
  }

  std::vector<std::size_t> slot_of(
      n, std::numeric_limits<std::size_t>::max());
  std::size_t scheduled = 0U;
  for (std::size_t slot_index = 0U;
       slot_index < schedule.slots.size(); ++slot_index) {
    const auto& slot = schedule.slots[slot_index];
    if (!slot[0U].has_value() && slot[1U].has_value()) {
      return false;
    }
    if (slot[0U].has_value() && slot[1U].has_value() &&
        *slot[0U] == *slot[1U]) {
      return false;
    }

    for (const auto task : slot) {
      if (!task.has_value()) {
        continue;
      }
      if (*task >= n ||
          slot_of[*task] != std::numeric_limits<std::size_t>::max()) {
        return false;
      }
      slot_of[*task] = slot_index;
      ++scheduled;
    }
  }

  if (scheduled != n) {
    return false;
  }

  for (std::size_t task = 0U; task < n; ++task) {
    for (const std::size_t successor : successors[task]) {
      if (successor >= n ||
          !(slot_of[task] < slot_of[successor]) ||
          !(schedule.labels[task] > schedule.labels[successor])) {
        return false;
      }
    }
  }
  return true;
}

inline std::size_t brute_optimal_makespan(
    const std::vector<std::vector<std::size_t>>& successors) {
  const std::size_t n = successors.size();
  if (n == 0U) {
    return 0U;
  }
  if (n > 20U) {
    throw std::invalid_argument(
        "brute two-processor scheduler limited to 20 tasks");
  }

  std::vector<std::uint64_t> predecessor_mask(n, UINT64_C(0));
  for (std::size_t task = 0U; task < n; ++task) {
    for (const std::size_t successor : successors[task]) {
      predecessor_mask[successor] |= UINT64_C(1) << task;
    }
  }

  const std::uint64_t all =
      (UINT64_C(1) << n) - UINT64_C(1);
  const std::size_t unknown =
      std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> memo(
      static_cast<std::size_t>(all + UINT64_C(1)), unknown);

  const std::function<std::size_t(std::uint64_t)> solve =
      [&](const std::uint64_t done) -> std::size_t {
        if (done == all) {
          return 0U;
        }
        const std::size_t index =
            static_cast<std::size_t>(done);
        if (memo[index] != unknown) {
          return memo[index];
        }

        std::vector<std::size_t> ready;
        for (std::size_t task = 0U; task < n; ++task) {
          const std::uint64_t bit = UINT64_C(1) << task;
          if ((done & bit) != 0U) {
            continue;
          }
          if ((predecessor_mask[task] & ~done) == 0U) {
            ready.push_back(task);
          }
        }

        if (ready.empty()) {
          throw std::invalid_argument(
              "brute scheduler received a cyclic graph");
        }

        std::size_t best = unknown;
        for (std::size_t first = 0U;
             first < ready.size(); ++first) {
          const std::uint64_t first_bit =
              UINT64_C(1) << ready[first];
          best = std::min(
              best, 1U + solve(done | first_bit));

          for (std::size_t second = first + 1U;
               second < ready.size(); ++second) {
            const std::uint64_t second_bit =
                UINT64_C(1) << ready[second];
            best = std::min(
                best,
                1U + solve(done | first_bit | second_bit));
          }
        }

        memo[index] = best;
        return best;
      };

  return solve(UINT64_C(0));
}

inline std::vector<std::vector<std::size_t>> random_dag(
    std::mt19937_64& random, const std::size_t n) {
  std::vector<std::size_t> order(n);
  for (std::size_t index = 0U; index < n; ++index) {
    order[index] = index;
  }
  std::shuffle(order.begin(), order.end(), random);

  std::vector<std::vector<std::size_t>> successors(n);
  for (std::size_t left = 0U; left < n; ++left) {
    for (std::size_t right = left + 1U; right < n; ++right) {
      if ((random() % 4U) == 0U) {
        successors[order[left]].push_back(order[right]);
      }
    }
  }
  return successors;
}

}  // namespace coffman_graham_test_detail

TEST_CASE(coffman_graham_empty_and_single_task) {
  const std::vector<std::vector<std::size_t>> empty_graph;
  const auto empty =
      coffman_graham_two_processor_schedule(empty_graph);
  REQUIRE(empty.labels.empty());
  REQUIRE(empty.slots.empty());
  REQUIRE_EQ(empty.makespan(), 0U);

  const std::vector<std::vector<std::size_t>> one_graph(1U);
  const auto one =
      coffman_graham_two_processor_schedule(one_graph);
  REQUIRE_EQ(one.labels, std::vector<std::size_t>{1U});
  REQUIRE_EQ(one.makespan(), 1U);
  REQUIRE(one.slots[0U][0U] ==
          std::optional<std::size_t>(0U));
  REQUIRE(!one.slots[0U][1U].has_value());
}

TEST_CASE(coffman_graham_lexicographic_successor_labels) {
  // 0 -> {2,3}, 1 -> {3}; 2 and 3 are sinks.
  //
  // Sinks tie on empty signatures, so node 2 receives label 1 and node 3
  // receives label 2. The then-eligible signatures are:
  //   node 1: [2]
  //   node 0: [2,1]
  // and [2] is lexicographically smaller than [2,1].
  const std::vector<std::vector<std::size_t>> graph{
      {2U, 3U},
      {3U},
      {},
      {},
  };

  const auto schedule =
      coffman_graham_two_processor_schedule(graph);

  REQUIRE_EQ(schedule.labels,
             (std::vector<std::size_t>{4U, 3U, 1U, 2U}));
  REQUIRE_EQ(schedule.makespan(), 2U);
  REQUIRE(
      coffman_graham_test_detail::valid_schedule(graph, schedule));
}

TEST_CASE(coffman_graham_rejects_invalid_graphs) {
  const std::vector<std::vector<std::size_t>> out_of_range{
      {1U},
  };
  REQUIRE_THROWS_AS(
      coffman_graham_two_processor_schedule(out_of_range),
      std::invalid_argument);

  const std::vector<std::vector<std::size_t>> duplicate_edge{
      {1U, 1U},
      {},
  };
  REQUIRE_THROWS_AS(
      coffman_graham_two_processor_schedule(duplicate_edge),
      std::invalid_argument);

  const std::vector<std::vector<std::size_t>> cycle{
      {1U},
      {2U},
      {0U},
  };
  REQUIRE_THROWS_AS(
      coffman_graham_two_processor_schedule(cycle),
      std::invalid_argument);

  const std::vector<std::vector<std::size_t>> self_cycle{
      {0U},
  };
  REQUIRE_THROWS_AS(
      coffman_graham_two_processor_schedule(self_cycle),
      std::invalid_argument);
}

TEST_CASE(coffman_graham_fixed_precedence_shapes_are_optimal) {
  using namespace coffman_graham_test_detail;

  const std::vector<std::vector<std::vector<std::size_t>>> cases{
      {
          {2U},
          {2U},
          {3U, 4U},
          {5U},
          {5U},
          {},
      },
      {
          {3U, 4U},
          {4U, 5U},
          {5U},
          {6U},
          {6U},
          {6U},
          {},
      },
      {
          {4U},
          {4U},
          {5U},
          {5U},
          {6U},
          {6U},
          {},
      },
  };

  for (const auto& graph : cases) {
    const auto schedule =
        coffman_graham_two_processor_schedule(graph);
    REQUIRE(valid_schedule(graph, schedule));
    REQUIRE_EQ(schedule.makespan(),
               brute_optimal_makespan(graph));
    REQUIRE(schedule ==
            coffman_graham_two_processor_schedule(graph));
  }
}

TEST_CASE(coffman_graham_random_dags_match_exhaustive_optimum) {
  using namespace coffman_graham_test_detail;

  std::mt19937_64 random(UINT64_C(0xC0FF6A4A2));
  for (std::size_t trial = 0U; trial < 700U; ++trial) {
    const std::size_t n =
        static_cast<std::size_t>(random() % 9U);
    const auto graph = random_dag(random, n);

    const auto schedule =
        coffman_graham_two_processor_schedule(graph);

    REQUIRE(valid_schedule(graph, schedule));
    REQUIRE_EQ(schedule.makespan(),
               brute_optimal_makespan(graph));
    REQUIRE(schedule ==
            coffman_graham_two_processor_schedule(graph));
  }
}
