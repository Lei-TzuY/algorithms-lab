#pragma once

#include "algorithms/data_structures/partial_retroactive_queue.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

namespace partial_retroactive_queue_test_detail {
using Queue = algorithms::data_structures::PartialRetroactiveQueue;

struct OracleEvent {
  bool enqueue{};
  std::int64_t value{};
};

inline bool replay_valid(const std::map<std::uint64_t, OracleEvent>& events,
                         std::deque<std::int64_t>* queue_out = nullptr) {
  std::deque<std::int64_t> queue;
  for (const auto& [timestamp, event] : events) {
    static_cast<void>(timestamp);
    if (event.enqueue) {
      queue.push_back(event.value);
    } else {
      if (queue.empty()) {
        return false;
      }
      queue.pop_front();
    }
  }
  if (queue_out != nullptr) {
    *queue_out = queue;
  }
  return true;
}

inline void require_matches(const Queue& queue,
                            const std::map<std::uint64_t, OracleEvent>& events) {
  std::deque<std::int64_t> expected;
  REQUIRE(replay_valid(events, &expected));
  REQUIRE(queue.valid_structure());
  REQUIRE_EQ(queue.event_count(), events.size());
  REQUIRE_EQ(queue.size(), expected.size());
  REQUIRE_EQ(queue.empty(), expected.empty());
  if (expected.empty()) {
    REQUIRE_THROWS_AS(queue.front(), std::out_of_range);
    REQUIRE_THROWS_AS(queue.back(), std::out_of_range);
  } else {
    REQUIRE_EQ(queue.front(), expected.front());
    REQUIRE_EQ(queue.back(), expected.back());
  }
}
}  // namespace partial_retroactive_queue_test_detail

TEST_CASE(partial_retroactive_queue_edits_past_without_replay_api) {
  using namespace partial_retroactive_queue_test_detail;
  Queue queue;
  queue.insert_enqueue(10U, 100);
  queue.insert_enqueue(20U, 200);
  queue.insert_dequeue(30U);
  REQUIRE_EQ(queue.size(), 1U);
  REQUIRE_EQ(queue.front(), 200);

  queue.insert_enqueue(5U, 50);
  REQUIRE_EQ(queue.size(), 2U);
  REQUIRE_EQ(queue.front(), 100);
  REQUIRE_EQ(queue.back(), 200);

  queue.insert_dequeue(15U);
  REQUIRE_EQ(queue.size(), 1U);
  REQUIRE_EQ(queue.front(), 200);

  queue.erase(15U);
  REQUIRE_EQ(queue.size(), 2U);
  REQUIRE_EQ(queue.front(), 100);
  REQUIRE(queue.valid_structure());
}

TEST_CASE(partial_retroactive_queue_event_limit_is_transactional) {
  using namespace partial_retroactive_queue_test_detail;
  REQUIRE_THROWS_AS(
      Queue(Queue::kMaximumEventLimit + 1U), std::invalid_argument);
  Queue queue(2U);
  queue.insert_enqueue(1U, 10);
  queue.insert_enqueue(2U, 20);
  REQUIRE_THROWS_AS(queue.insert_enqueue(3U, 30), std::length_error);
  REQUIRE_EQ(queue.event_count(), 2U);
  REQUIRE_EQ(queue.front(), 10);
  REQUIRE_EQ(queue.back(), 20);
  REQUIRE(queue.valid_structure());
}

TEST_CASE(partial_retroactive_queue_transactional_invalid_edits) {
  using namespace partial_retroactive_queue_test_detail;
  Queue queue;
  REQUIRE_THROWS_AS(queue.insert_dequeue(10U), std::invalid_argument);
  REQUIRE_EQ(queue.event_count(), 0U);
  REQUIRE(queue.valid_structure());

  queue.insert_enqueue(10U, 7);
  queue.insert_dequeue(20U);
  REQUIRE_THROWS_AS(queue.erase(10U), std::invalid_argument);
  REQUIRE(queue.contains_timestamp(10U));
  REQUIRE_EQ(queue.event_count(), 2U);
  REQUIRE(queue.empty());
  REQUIRE(queue.valid_structure());

  REQUIRE_THROWS_AS(queue.insert_enqueue(10U, 8), std::invalid_argument);
  REQUIRE_THROWS_AS(queue.erase(999U), std::out_of_range);
  REQUIRE_EQ(queue.event_count(), 2U);
}

TEST_CASE(partial_retroactive_queue_erased_dequeue_resurrects_fifo_prefix) {
  using namespace partial_retroactive_queue_test_detail;
  Queue queue;
  queue.insert_enqueue(1U, 11);
  queue.insert_enqueue(2U, 22);
  queue.insert_dequeue(3U);
  REQUIRE_EQ(queue.front(), 22);
  queue.erase(3U);
  REQUIRE_EQ(queue.front(), 11);
  REQUIRE_EQ(queue.back(), 22);

  queue.insert_enqueue(0U, std::numeric_limits<std::int64_t>::min());
  queue.insert_enqueue(std::numeric_limits<std::uint64_t>::max(),
                       std::numeric_limits<std::int64_t>::max());
  REQUIRE_EQ(queue.front(), std::numeric_limits<std::int64_t>::min());
  REQUIRE_EQ(queue.back(), std::numeric_limits<std::int64_t>::max());
  REQUIRE(queue.valid_structure());
}

TEST_CASE(partial_retroactive_queue_sorted_timestamps_stay_balanced) {
  using namespace partial_retroactive_queue_test_detail;
  Queue queue;
  constexpr std::size_t count = 4096U;
  for (std::size_t index = 0U; index < count; ++index) {
    queue.insert_enqueue(static_cast<std::uint64_t>(index),
                         static_cast<std::int64_t>(index));
  }
  REQUIRE(queue.valid_structure());
  REQUIRE(queue.tree_height() <= 20U);
  for (std::size_t index = 0U; index < count; index += 2U) {
    queue.erase(static_cast<std::uint64_t>(index));
  }
  REQUIRE(queue.valid_structure());
  REQUIRE(queue.tree_height() <= 20U);
  REQUIRE_EQ(queue.size(), count / 2U);
}

TEST_CASE(partial_retroactive_queue_randomized_differential_replay) {
  using namespace partial_retroactive_queue_test_detail;
  Queue queue;
  std::map<std::uint64_t, OracleEvent> oracle;
  std::mt19937_64 rng(0xA11CE5EEDULL);
  std::uniform_int_distribution<std::uint64_t> timestamp_dist(0U, 511U);
  std::uniform_int_distribution<std::int64_t> value_dist(-1'000'000, 1'000'000);
  std::uniform_int_distribution<int> action_dist(0, 99);

  for (std::size_t step = 0U; step < 12'000U; ++step) {
    const int action = action_dist(rng);
    const std::uint64_t timestamp = timestamp_dist(rng);
    if (action < 45) {
      const std::int64_t value = value_dist(rng);
      if (oracle.contains(timestamp)) {
        REQUIRE_THROWS_AS(queue.insert_enqueue(timestamp, value),
                          std::invalid_argument);
      } else {
        oracle.emplace(timestamp, OracleEvent{true, value});
        queue.insert_enqueue(timestamp, value);
      }
    } else if (action < 70) {
      if (oracle.contains(timestamp)) {
        REQUIRE_THROWS_AS(queue.insert_dequeue(timestamp), std::invalid_argument);
      } else {
        auto candidate = oracle;
        candidate.emplace(timestamp, OracleEvent{false, 0});
        if (replay_valid(candidate)) {
          queue.insert_dequeue(timestamp);
          oracle = std::move(candidate);
        } else {
          REQUIRE_THROWS_AS(queue.insert_dequeue(timestamp),
                            std::invalid_argument);
        }
      }
    } else {
      const auto found = oracle.find(timestamp);
      if (found == oracle.end()) {
        REQUIRE_THROWS_AS(queue.erase(timestamp), std::out_of_range);
      } else {
        auto candidate = oracle;
        candidate.erase(timestamp);
        if (replay_valid(candidate)) {
          queue.erase(timestamp);
          oracle = std::move(candidate);
        } else {
          REQUIRE_THROWS_AS(queue.erase(timestamp), std::invalid_argument);
        }
      }
    }
    require_matches(queue, oracle);
  }
}
