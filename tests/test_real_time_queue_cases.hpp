#pragma once

#include "algorithms/data_structures/real_time_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <random>
#include <stdexcept>
#include <vector>

namespace real_time_queue_test_detail {

using algorithms::data_structures::RealTimeQueue;

inline void require_state(const RealTimeQueue& queue,
                          const std::deque<std::int64_t>& oracle) {
  REQUIRE(queue.valid_invariants());
  REQUIRE_EQ(queue.size(), oracle.size());
  REQUIRE_EQ(queue.empty(), oracle.empty());
  REQUIRE(queue.last_rotation_work() <= 2U);

  if (oracle.empty()) {
    REQUIRE_THROWS_AS(queue.front(), std::out_of_range);
  } else {
    REQUIRE_EQ(queue.front(), oracle.front());
  }
}

inline void require_full_sequence(RealTimeQueue queue,
                                  std::deque<std::int64_t> oracle) {
  require_state(queue, oracle);
  while (!oracle.empty()) {
    REQUIRE_EQ(queue.front(), oracle.front());
    queue = queue.pop_front();
    oracle.pop_front();
    require_state(queue, oracle);
  }
}

}  // namespace real_time_queue_test_detail

TEST_CASE(real_time_queue_fifo_and_rotation_schedule_bound) {
  using namespace real_time_queue_test_detail;

  RealTimeQueue queue;
  std::deque<std::int64_t> oracle;
  require_state(queue, oracle);
  REQUIRE_THROWS_AS(queue.pop_front(), std::out_of_range);

  for (std::int64_t value = 0; value < 4096; ++value) {
    queue = queue.push_back(value);
    oracle.push_back(value);
    require_state(queue, oracle);

    if ((value % 5) == 0 && !oracle.empty()) {
      queue = queue.pop_front();
      oracle.pop_front();
      require_state(queue, oracle);
    }
  }

  require_full_sequence(queue, oracle);
}

TEST_CASE(real_time_queue_persistence_preserves_old_versions) {
  using namespace real_time_queue_test_detail;

  const RealTimeQueue empty;
  const RealTimeQueue one = empty.push_back(10);
  const RealTimeQueue two = one.push_back(20);
  const RealTimeQueue three = two.push_back(30);
  const RealTimeQueue popped = three.pop_front();
  const RealTimeQueue branch = one.push_back(99);

  require_full_sequence(empty, {});
  require_full_sequence(one, {10});
  require_full_sequence(two, {10, 20});
  require_full_sequence(three, {10, 20, 30});
  require_full_sequence(popped, {20, 30});
  require_full_sequence(branch, {10, 99});
}

TEST_CASE(real_time_queue_repeated_rotation_overlap_remains_fifo) {
  using namespace real_time_queue_test_detail;

  RealTimeQueue queue;
  std::deque<std::int64_t> oracle;

  for (std::int64_t round = 0; round < 1500; ++round) {
    queue = queue.push_back(round * 3);
    oracle.push_back(round * 3);
    queue = queue.push_back(round * 3 + 1);
    oracle.push_back(round * 3 + 1);

    if ((round & 1) == 0) {
      queue = queue.push_back(round * 3 + 2);
      oracle.push_back(round * 3 + 2);
    }

    if ((round % 3) != 0 && !oracle.empty()) {
      REQUIRE_EQ(queue.front(), oracle.front());
      queue = queue.pop_front();
      oracle.pop_front();
    }
    if ((round % 11) == 0 && !oracle.empty()) {
      REQUIRE_EQ(queue.front(), oracle.front());
      queue = queue.pop_front();
      oracle.pop_front();
    }

    require_state(queue, oracle);
  }

  require_full_sequence(queue, oracle);
}

TEST_CASE(real_time_queue_randomized_branching_versions_match_deque_oracle) {
  using namespace real_time_queue_test_detail;

  std::mt19937_64 random(0xA11CE5EEDULL);
  std::vector<RealTimeQueue> versions(1U);
  std::vector<std::deque<std::int64_t>> oracles(1U);

  for (std::size_t step = 0U; step < 6000U; ++step) {
    const std::size_t base =
        static_cast<std::size_t>(random() % versions.size());

    RealTimeQueue next = versions[base];
    std::deque<std::int64_t> oracle = oracles[base];

    if (oracle.empty() || (random() % 5U) < 3U) {
      const std::int64_t value =
          static_cast<std::int64_t>(random() % 2000001ULL) - 1000000;
      next = next.push_back(value);
      oracle.push_back(value);
    } else {
      REQUIRE_EQ(next.front(), oracle.front());
      next = next.pop_front();
      oracle.pop_front();
    }

    require_state(next, oracle);
    versions.push_back(next);
    oracles.push_back(oracle);

    if ((step % 47U) == 0U) {
      require_full_sequence(next, oracle);
      const std::size_t historical =
          static_cast<std::size_t>(random() % versions.size());
      require_full_sequence(versions[historical], oracles[historical]);
    }
  }
}
