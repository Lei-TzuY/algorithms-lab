#include "algorithms/graphs/ssa_destruction.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

using Token = std::uint64_t;

struct LocationKey {
  SsaCopyLocationKind kind;
  Variable variable;
  std::size_t version;
  std::size_t temporary;
  friend bool operator<(const LocationKey& a, const LocationKey& b) {
    return std::tie(a.kind, a.variable, a.version, a.temporary) <
           std::tie(b.kind, b.variable, b.version, b.temporary);
  }
};

LocationKey key(const SsaCopyLocation& location) {
  return {location.kind, location.value.variable, location.value.version,
          location.temporary};
}

void verify_schedule(const std::vector<SsaParallelCopy>& copies,
                     const std::vector<SsaScheduledMove>& moves) {
  std::map<LocationKey, Token> initial;
  Token next = 1U;
  for (const SsaParallelCopy& copy : copies) {
    for (const SsaValue value : {copy.destination, copy.source}) {
      const LocationKey location =
          key(SsaCopyLocation::from_value(value));
      if (!initial.contains(location)) {
        initial[location] = next++;
      }
    }
  }
  auto state = initial;
  for (const SsaScheduledMove& move : moves) {
    const LocationKey source = key(move.source);
    const LocationKey destination = key(move.destination);
    if (!state.contains(source)) {
      // A temporary source must have been defined by an earlier move.
      REQUIRE(move.source.kind != SsaCopyLocationKind::temporary);
      state[source] = next++;
    }
    state[destination] = state[source];
  }
  for (const SsaParallelCopy& copy : copies) {
    REQUIRE_EQ(
        state.at(key(SsaCopyLocation::from_value(copy.destination))),
        initial.at(key(SsaCopyLocation::from_value(copy.source))));
  }
}

std::vector<std::vector<Vertex>> unique_predecessors(const Graph& graph) {
  std::vector<std::vector<Vertex>> predecessors(graph.vertex_count());
  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    for (const Edge& edge : graph.neighbors(block)) {
      predecessors[edge.to].push_back(block);
    }
  }
  for (std::vector<Vertex>& list : predecessors) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }
  return predecessors;
}

std::vector<std::vector<Vertex>> unique_successors(const Graph& graph) {
  std::vector<std::vector<Vertex>> successors(graph.vertex_count());
  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    for (const Edge& edge : graph.neighbors(block)) {
      successors[block].push_back(edge.to);
    }
    std::sort(successors[block].begin(), successors[block].end());
    successors[block].erase(
        std::unique(successors[block].begin(), successors[block].end()),
        successors[block].end());
  }
  return successors;
}

TEST_CASE(parallel_copy_scheduler_handles_cycles_and_is_deterministic) {
  const std::vector<SsaParallelCopy> copies{
      {{0U, 1U}, {1U, 1U}},
      {{1U, 1U}, {2U, 1U}},
      {{2U, 1U}, {0U, 1U}},
      {{3U, 1U}, {2U, 1U}}};
  const SsaCopySchedule first = schedule_parallel_copies(copies);
  const SsaCopySchedule second = schedule_parallel_copies(copies);
  REQUIRE(first == second);
  REQUIRE_EQ(first.temporary_count, 1U);
  verify_schedule(copies, first.moves);

  REQUIRE_THROWS_AS(
      schedule_parallel_copies(
          {{{0U, 1U}, {1U, 1U}}, {{0U, 1U}, {2U, 1U}}}),
      std::invalid_argument);
}

TEST_CASE(ssa_destruction_splits_each_physical_critical_edge) {
  Graph graph(5U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  graph.add_edge(1U, 3U, 7);
  graph.add_edge(1U, 3U, 9);
  graph.add_edge(1U, 4U);
  graph.add_edge(2U, 3U);

  SsaProgram program;
  program.start = 0U;
  program.variable_count = 1U;
  program.initial_values = {{0U, 0U}};
  program.blocks.resize(5U);
  for (SsaBlock& block : program.blocks) {
    block.reachable = true;
  }
  program.blocks[1U].instructions.push_back({{}, SsaValue{0U, 1U}});
  program.blocks[2U].instructions.push_back({{}, SsaValue{0U, 2U}});
  program.blocks[3U].phis.push_back(
      {0U,
       {0U, 3U},
       {{1U, {0U, 1U}}, {2U, {0U, 2U}}}});
  program.blocks[3U].instructions.push_back(
      {{{0U, 3U}}, std::nullopt});

  const OutOfSsaProgram lowered = destroy_ssa(graph, program);
  REQUIRE_EQ(lowered.graph.vertex_count(), 7U);
  REQUIRE_EQ(lowered.edge_lowerings.size(), 2U);

  const auto critical = std::find_if(
      lowered.edge_lowerings.begin(), lowered.edge_lowerings.end(),
      [](const SsaEdgeLowering& lowering) {
        return lowering.predecessor == 1U && lowering.successor == 3U;
      });
  REQUIRE(critical != lowered.edge_lowerings.end());
  REQUIRE(critical->placement == SsaCopyPlacement::split_blocks);
  REQUIRE_EQ(critical->execution_blocks.size(), 2U);
  for (const Vertex split : critical->execution_blocks) {
    REQUIRE(!lowered.blocks[split].original_block.has_value());
    REQUIRE(lowered.blocks[split].entry_moves == critical->schedule);
    REQUIRE_EQ(lowered.graph.neighbors(split).size(), 1U);
    REQUIRE_EQ(lowered.graph.neighbors(split)[0U].to, 3U);
  }
  verify_schedule(critical->parallel_copies, critical->schedule);

  const auto noncritical = std::find_if(
      lowered.edge_lowerings.begin(), lowered.edge_lowerings.end(),
      [](const SsaEdgeLowering& lowering) {
        return lowering.predecessor == 2U && lowering.successor == 3U;
      });
  REQUIRE(noncritical != lowered.edge_lowerings.end());
  REQUIRE(noncritical->placement == SsaCopyPlacement::predecessor_exit);
  REQUIRE(lowered.blocks[2U].exit_moves == noncritical->schedule);
}

TEST_CASE(ssa_destruction_uses_successor_entry_for_unique_predecessor) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  SsaProgram program{0U, 1U, {{0U, 0U}}, std::vector<SsaBlock>(3U)};
  for (SsaBlock& block : program.blocks) {
    block.reachable = true;
  }
  program.blocks[1U].phis.push_back(
      {0U, {0U, 1U}, {{0U, {0U, 0U}}}});
  program.blocks[1U].instructions.push_back(
      {{{0U, 1U}}, std::nullopt});
  const OutOfSsaProgram lowered = destroy_ssa(graph, program);
  REQUIRE_EQ(lowered.edge_lowerings.size(), 1U);
  REQUIRE(lowered.edge_lowerings[0U].placement ==
          SsaCopyPlacement::successor_entry);
  REQUIRE(lowered.blocks[1U].entry_moves ==
          lowered.edge_lowerings[0U].schedule);
}

TEST_CASE(ssa_destruction_rejects_malformed_program_shapes) {
  Graph undirected(2U, false);
  SsaProgram empty{0U, 0U, {}, std::vector<SsaBlock>(2U)};
  REQUIRE_THROWS_AS(destroy_ssa(undirected, empty), std::invalid_argument);

  Graph graph(2U, true);
  graph.add_edge(0U, 1U);
  SsaProgram bad{0U, 1U, {{0U, 0U}}, std::vector<SsaBlock>(2U)};
  bad.blocks[0U].reachable = true;
  bad.blocks[1U].reachable = true;
  bad.blocks[1U].phis.push_back({0U, {0U, 1U}, {}});
  REQUIRE_THROWS_AS(destroy_ssa(graph, bad), std::invalid_argument);

  Graph loop_entry(1U, true);
  loop_entry.add_edge(0U, 0U);
  SsaProgram entry{0U, 0U, {}, std::vector<SsaBlock>(1U)};
  entry.blocks[0U].reachable = true;
  REQUIRE_THROWS_AS(destroy_ssa(loop_entry, entry), std::invalid_argument);
}

TEST_CASE(random_parallel_copy_schedules_match_simultaneous_oracle) {
  std::mt19937_64 rng(0x47C0FFEEULL);
  for (std::size_t trial = 0U; trial < 1500U; ++trial) {
    const std::size_t count =
        1U + static_cast<std::size_t>(rng() % 8U);
    std::vector<std::size_t> permutation(count);
    std::iota(permutation.begin(), permutation.end(), 0U);
    std::shuffle(permutation.begin(), permutation.end(), rng);
    std::vector<SsaParallelCopy> copies;
    for (std::size_t index = 0U; index < count; ++index) {
      copies.push_back(
          {SsaValue{index, 1U}, SsaValue{permutation[index], 1U}});
    }
    const SsaCopySchedule schedule = schedule_parallel_copies(copies);
    REQUIRE(schedule.temporary_count <= count);
    verify_schedule(copies, schedule.moves);
  }
}

TEST_CASE(random_dag_phi_bundles_have_correct_placement_and_semantics) {
  std::mt19937_64 rng(0x47D357A0ULL);
  for (std::size_t trial = 0U; trial < 450U; ++trial) {
    const std::size_t vertex_count =
        2U + static_cast<std::size_t>(rng() % 8U);
    const std::size_t variable_count =
        1U + static_cast<std::size_t>(rng() % 4U);
    Graph graph(vertex_count, true);

    // The chain guarantees start reachability. Extra forward edges create joins,
    // branches, critical edges, and occasional parallel physical arcs.
    for (Vertex block = 0U; block + 1U < vertex_count; ++block) {
      graph.add_edge(block, block + 1U);
    }
    for (Vertex from = 0U; from < vertex_count; ++from) {
      for (Vertex to = from + 2U; to < vertex_count; ++to) {
        if ((rng() % 5U) == 0U) {
          graph.add_edge(from, to, static_cast<Weight>(rng() % 17U));
          if ((rng() % 7U) == 0U) {
            graph.add_edge(from, to, static_cast<Weight>(rng() % 17U));
          }
        }
      }
    }
    const auto predecessors = unique_predecessors(graph);
    const auto successors = unique_successors(graph);

    SsaProgram program;
    program.start = 0U;
    program.variable_count = variable_count;
    program.blocks.resize(vertex_count);
    for (SsaBlock& block : program.blocks) {
      block.reachable = true;
    }
    for (Variable variable = 0U; variable < variable_count; ++variable) {
      program.initial_values.push_back({variable, 0U});
    }

    std::vector<std::size_t> next_version(variable_count, 1U);
    for (Vertex block = 1U; block < vertex_count; ++block) {
      if (predecessors[block].size() < 2U) {
        continue;
      }
      for (Variable variable = 0U; variable < variable_count; ++variable) {
        if ((rng() % 2U) == 0U) {
          continue;
        }
        SsaPhi phi;
        phi.variable = variable;
        phi.result = {variable, next_version[variable]++};
        for (const Vertex predecessor : predecessors[block]) {
          phi.incoming.push_back({predecessor, {variable, 0U}});
        }
        program.blocks[block].phis.push_back(std::move(phi));
      }
    }

    const OutOfSsaProgram lowered = destroy_ssa(graph, program);
    std::size_t expected_splits = 0U;
    for (const SsaEdgeLowering& site : lowered.edge_lowerings) {
      REQUIRE(!site.parallel_copies.empty());
      verify_schedule(site.parallel_copies, site.schedule);
      if (successors[site.predecessor].size() == 1U) {
        REQUIRE(site.placement == SsaCopyPlacement::predecessor_exit);
        REQUIRE_EQ(site.execution_blocks.size(), 1U);
        REQUIRE_EQ(site.execution_blocks[0U], site.predecessor);
      } else if (predecessors[site.successor].size() == 1U) {
        REQUIRE(site.placement == SsaCopyPlacement::successor_entry);
        REQUIRE_EQ(site.execution_blocks.size(), 1U);
        REQUIRE_EQ(site.execution_blocks[0U], site.successor);
      } else {
        REQUIRE(site.placement == SsaCopyPlacement::split_blocks);
        std::size_t multiplicity = 0U;
        for (const Edge& edge : graph.neighbors(site.predecessor)) {
          if (edge.to == site.successor) {
            ++multiplicity;
          }
        }
        REQUIRE_EQ(site.execution_blocks.size(), multiplicity);
        expected_splits += multiplicity;
      }
    }
    REQUIRE_EQ(lowered.graph.vertex_count(),
               vertex_count + expected_splits);

    // Every phi/predecessor pair is represented by exactly one logical lowering
    // site, regardless of parallel physical arcs.
    for (Vertex block = 0U; block < vertex_count; ++block) {
      if (program.blocks[block].phis.empty()) {
        continue;
      }
      for (const Vertex predecessor : predecessors[block]) {
        const auto count = static_cast<std::size_t>(std::count_if(
            lowered.edge_lowerings.begin(), lowered.edge_lowerings.end(),
            [predecessor, block](const SsaEdgeLowering& site) {
              return site.predecessor == predecessor &&
                     site.successor == block;
            }));
        REQUIRE_EQ(count, 1U);
      }
    }
  }
}

}  // namespace
