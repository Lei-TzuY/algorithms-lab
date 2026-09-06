#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/data_structures/rollback_disjoint_set_union.hpp"

namespace {

using algorithms::data_structures::RollbackDisjointSetUnion;

struct OracleState {
  std::vector<std::size_t> component;
  std::vector<std::size_t> component_size;
  std::size_t components = 0;
};

class EdgeOracle {
 public:
  explicit EdgeOracle(std::size_t size) : size_(size) {}

  [[nodiscard]] std::size_t snapshot() const noexcept { return edges_.size(); }

  bool unite(std::size_t a, std::size_t b) {
    const OracleState state = analyze();
    if (state.component[a] == state.component[b]) {
      return false;
    }
    edges_.emplace_back(a, b);
    return true;
  }

  void rollback(std::size_t target) { edges_.resize(target); }

  [[nodiscard]] OracleState analyze() const {
    std::vector<std::vector<std::size_t>> adjacency(size_);
    for (const auto& [a, b] : edges_) {
      adjacency[a].push_back(b);
      adjacency[b].push_back(a);
    }

    OracleState state;
    state.component.assign(size_, size_);
    std::vector<std::size_t> queue;
    for (std::size_t start = 0; start < size_; ++start) {
      if (state.component[start] != size_) {
        continue;
      }

      const std::size_t component_id = state.components;
      ++state.components;
      queue.clear();
      queue.push_back(start);
      state.component[start] = component_id;
      std::size_t cursor = 0;
      while (cursor < queue.size()) {
        const std::size_t vertex = queue[cursor++];
        for (std::size_t neighbor : adjacency[vertex]) {
          if (state.component[neighbor] == size_) {
            state.component[neighbor] = component_id;
            queue.push_back(neighbor);
          }
        }
      }
      state.component_size.push_back(queue.size());
    }
    return state;
  }

 private:
  std::size_t size_;
  std::vector<std::pair<std::size_t, std::size_t>> edges_;
};

void require_matches(const RollbackDisjointSetUnion& dsu,
                     const EdgeOracle& oracle) {
  const OracleState state = oracle.analyze();
  REQUIRE_EQ(dsu.size(), state.component.size());
  REQUIRE_EQ(dsu.components(), state.components);

  for (std::size_t vertex = 0; vertex < dsu.size(); ++vertex) {
    REQUIRE_EQ(dsu.component_size(vertex),
               state.component_size[state.component[vertex]]);
    for (std::size_t other = 0; other < dsu.size(); ++other) {
      REQUIRE_EQ(dsu.connected(vertex, other),
                 state.component[vertex] == state.component[other]);
    }
  }
}

}  // namespace

TEST_CASE(rollback_dsu_restores_nested_snapshots_exactly) {
  RollbackDisjointSetUnion dsu(6);
  REQUIRE_EQ(dsu.components(), std::size_t{6});
  const auto initial = dsu.snapshot();
  REQUIRE_EQ(initial, std::size_t{0});

  REQUIRE(dsu.unite(0, 1));
  REQUIRE(dsu.unite(2, 3));
  const auto middle = dsu.snapshot();
  REQUIRE_EQ(dsu.components(), std::size_t{4});

  REQUIRE(dsu.unite(1, 2));
  REQUIRE(dsu.unite(4, 5));
  REQUIRE_EQ(dsu.component_size(0), std::size_t{4});
  REQUIRE_EQ(dsu.components(), std::size_t{2});

  dsu.rollback(middle);
  REQUIRE(dsu.connected(0, 1));
  REQUIRE(dsu.connected(2, 3));
  REQUIRE(!dsu.connected(0, 2));
  REQUIRE(!dsu.connected(4, 5));
  REQUIRE_EQ(dsu.components(), std::size_t{4});

  dsu.rollback(initial);
  REQUIRE_EQ(dsu.components(), std::size_t{6});
  for (std::size_t vertex = 0; vertex < dsu.size(); ++vertex) {
    REQUIRE_EQ(dsu.component_size(vertex), std::size_t{1});
  }
}

TEST_CASE(rollback_dsu_redundant_union_does_not_consume_history) {
  RollbackDisjointSetUnion dsu(4);
  REQUIRE(dsu.unite(0, 1));
  const auto before = dsu.snapshot();
  REQUIRE(!dsu.unite(1, 0));
  REQUIRE(!dsu.unite(0, 0));
  REQUIRE_EQ(dsu.snapshot(), before);

  dsu.rollback(0);
  REQUIRE(!dsu.connected(0, 1));
  REQUIRE_THROWS_AS(dsu.rollback(before), std::out_of_range);
}

TEST_CASE(rollback_dsu_validates_elements_and_empty_state) {
  RollbackDisjointSetUnion empty(0);
  REQUIRE_EQ(empty.size(), std::size_t{0});
  REQUIRE_EQ(empty.components(), std::size_t{0});
  REQUIRE_EQ(empty.snapshot(), std::size_t{0});
  empty.rollback(0);
  REQUIRE_THROWS_AS(empty.find(0), std::out_of_range);

  RollbackDisjointSetUnion dsu(3);
  REQUIRE_THROWS_AS(dsu.find(3), std::out_of_range);
  REQUIRE_THROWS_AS(dsu.connected(0, 3), std::out_of_range);
  REQUIRE_THROWS_AS(dsu.component_size(3), std::out_of_range);
  REQUIRE_THROWS_AS(dsu.unite(0, 3), std::out_of_range);
  REQUIRE_THROWS_AS(dsu.rollback(1), std::out_of_range);
}

TEST_CASE(rollback_dsu_matches_rebuilt_graph_oracle_randomized) {
  std::mt19937_64 rng(0x524f4c4c4241434bULL);
  std::uniform_int_distribution<int> size_distribution(1, 22);
  std::uniform_int_distribution<int> operation_distribution(0, 99);

  for (std::size_t trial = 0; trial < 150; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(size_distribution(rng));
    RollbackDisjointSetUnion dsu(size);
    EdgeOracle oracle(size);
    std::vector<std::size_t> snapshots{0};
    std::uniform_int_distribution<std::size_t> vertex_distribution(0, size - 1);

    for (std::size_t operation = 0; operation < 220; ++operation) {
      const int kind = operation_distribution(rng);
      if (kind < 58) {
        const std::size_t a = vertex_distribution(rng);
        const std::size_t b = vertex_distribution(rng);
        REQUIRE_EQ(dsu.unite(a, b), oracle.unite(a, b));
      } else if (kind < 78) {
        const std::size_t token = dsu.snapshot();
        REQUIRE_EQ(token, oracle.snapshot());
        snapshots.push_back(token);
      } else {
        std::uniform_int_distribution<std::size_t> snapshot_distribution(
            0, snapshots.size() - 1);
        const std::size_t token = snapshots[snapshot_distribution(rng)];
        dsu.rollback(token);
        oracle.rollback(token);
        snapshots.erase(
            std::remove_if(snapshots.begin(), snapshots.end(),
                           [token](std::size_t value) { return value > token; }),
            snapshots.end());
      }

      REQUIRE_EQ(dsu.snapshot(), oracle.snapshot());
      require_matches(dsu, oracle);
    }
  }
}
