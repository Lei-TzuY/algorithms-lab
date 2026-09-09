#include "algorithms/graphs/ssa_pruned.hpp"

#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <utility>
#include <vector>

using namespace algorithms::graphs;

namespace {

enum class SymbolKind { initial, phi, definition };

struct Symbol {
  SymbolKind kind;
  Vertex block;
  std::size_t instruction;
  Variable variable;
  friend bool operator==(const Symbol&, const Symbol&) = default;
};

std::vector<std::vector<Vertex>> unique_successors(const Graph& graph) {
  std::vector<std::vector<Vertex>> successors(graph.vertex_count());
  for (Vertex source = 0U; source < graph.vertex_count(); ++source) {
    for (const Edge& edge : graph.neighbors(source)) {
      successors[source].push_back(edge.to);
    }
    auto& list = successors[source];
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }
  return successors;
}

std::vector<std::vector<Vertex>> unique_predecessors(const Graph& graph) {
  std::vector<std::vector<Vertex>> predecessors(graph.vertex_count());
  for (Vertex source = 0U; source < graph.vertex_count(); ++source) {
    for (const Edge& edge : graph.neighbors(source)) {
      predecessors[edge.to].push_back(source);
    }
  }
  for (auto& list : predecessors) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }
  return predecessors;
}

bool oracle_live_in(
    const Graph& graph,
    const std::vector<std::vector<SsaInputInstruction>>& input,
    Vertex start_block, Variable variable) {
  const auto successors = unique_successors(graph);
  std::vector<unsigned char> visited(graph.vertex_count(), 0U);
  std::deque<Vertex> queue;
  queue.push_back(start_block);

  while (!queue.empty()) {
    const Vertex block = queue.front();
    queue.pop_front();
    if (visited[block] != 0U) {
      continue;
    }
    visited[block] = 1U;

    bool killed = false;
    for (const SsaInputInstruction& instruction : input[block]) {
      for (const Variable use : instruction.uses) {
        if (use == variable) {
          return true;
        }
      }
      if (instruction.definition == variable) {
        killed = true;
        break;
      }
    }
    if (killed) {
      continue;
    }
    for (const Vertex successor : successors[block]) {
      queue.push_back(successor);
    }
  }
  return false;
}

void verify_liveness_oracle(
    const Graph& graph, std::size_t variable_count,
    const std::vector<std::vector<SsaInputInstruction>>& input,
    const SsaLiveness& liveness) {
  const auto successors = unique_successors(graph);
  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    for (Variable variable = 0U; variable < variable_count; ++variable) {
      const bool expected_in = oracle_live_in(graph, input, block, variable);
      bool expected_out = false;
      for (const Vertex successor : successors[block]) {
        if (oracle_live_in(graph, input, successor, variable)) {
          expected_out = true;
          break;
        }
      }
      REQUIRE((liveness.live_in[block][variable] != 0U) == expected_in);
      REQUIRE((liveness.live_out[block][variable] != 0U) == expected_out);
    }
  }
}

TEST_CASE(pruned_ssa_dead_diamond_elides_minimal_phi) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  graph.add_edge(1U, 3U);
  graph.add_edge(2U, 3U);

  std::vector<std::vector<SsaInputInstruction>> blocks(4U);
  blocks[1U].push_back(SsaInputInstruction{{}, Variable{0U}});
  blocks[2U].push_back(SsaInputInstruction{{}, Variable{0U}});

  const SsaProgram minimal = construct_ssa(graph, 0U, 1U, blocks);
  const SsaLiveness liveness = analyze_ssa_liveness(graph, 0U, 1U, blocks);
  const SsaProgram pruned = construct_pruned_ssa(graph, 0U, 1U, blocks);

  REQUIRE_EQ(minimal.blocks[3U].phis.size(), 1U);
  REQUIRE_EQ(liveness.live_in[3U][0U], 0U);
  REQUIRE(pruned.blocks[3U].phis.empty());
  REQUIRE(construct_pruned_ssa(graph, 0U, 1U, blocks) == pruned);
}

TEST_CASE(pruned_ssa_live_diamond_retains_phi) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  graph.add_edge(1U, 3U);
  graph.add_edge(2U, 3U);

  std::vector<std::vector<SsaInputInstruction>> blocks(4U);
  blocks[1U].push_back(SsaInputInstruction{{}, Variable{0U}});
  blocks[2U].push_back(SsaInputInstruction{{}, Variable{0U}});
  blocks[3U].push_back(SsaInputInstruction{{0U}, std::nullopt});

  const SsaLiveness liveness = analyze_ssa_liveness(graph, 0U, 1U, blocks);
  const SsaProgram pruned = construct_pruned_ssa(graph, 0U, 1U, blocks);
  REQUIRE_EQ(liveness.live_in[3U][0U], 1U);
  REQUIRE_EQ(pruned.blocks[3U].phis.size(), 1U);
  REQUIRE_EQ(pruned.blocks[3U].phis[0U].incoming.size(), 2U);
  REQUIRE(pruned.blocks[3U].instructions[0U].uses[0U] ==
          pruned.blocks[3U].phis[0U].result);
}

TEST_CASE(pruned_ssa_loop_keeps_only_live_phi) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);
  graph.add_edge(2U, 1U);
  graph.add_edge(1U, 3U);

  std::vector<std::vector<SsaInputInstruction>> blocks(4U);
  blocks[1U].push_back(SsaInputInstruction{{0U}, std::nullopt});
  blocks[2U].push_back(SsaInputInstruction{{}, Variable{0U}});
  blocks[2U].push_back(SsaInputInstruction{{}, Variable{1U}});

  const SsaProgram minimal = construct_ssa(graph, 0U, 2U, blocks);
  const SsaLiveness liveness = analyze_ssa_liveness(graph, 0U, 2U, blocks);
  const SsaProgram pruned = construct_pruned_ssa(graph, 0U, 2U, blocks);

  REQUIRE_EQ(minimal.blocks[1U].phis.size(), 2U);
  REQUIRE_EQ(liveness.live_in[1U][0U], 1U);
  REQUIRE_EQ(liveness.live_in[1U][1U], 0U);
  REQUIRE_EQ(pruned.blocks[1U].phis.size(), 1U);
  REQUIRE_EQ(pruned.blocks[1U].phis[0U].variable, 0U);
  REQUIRE_EQ(pruned.blocks[1U].phis[0U].incoming.size(), 2U);
  REQUIRE(pruned.blocks[1U].instructions[0U].uses[0U] ==
          pruned.blocks[1U].phis[0U].result);
}

TEST_CASE(ssa_liveness_use_before_definition_and_local_kill) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);

  std::vector<std::vector<SsaInputInstruction>> blocks(3U);
  blocks[1U].push_back(SsaInputInstruction{{0U}, Variable{0U}});
  blocks[1U].push_back(SsaInputInstruction{{}, Variable{1U}});
  blocks[2U].push_back(SsaInputInstruction{{1U}, std::nullopt});

  const SsaLiveness liveness = analyze_ssa_liveness(graph, 0U, 2U, blocks);
  REQUIRE_EQ(liveness.live_in[1U][0U], 1U);
  REQUIRE_EQ(liveness.live_out[0U][0U], 1U);
  REQUIRE_EQ(liveness.live_out[1U][1U], 1U);
  REQUIRE_EQ(liveness.live_in[1U][1U], 0U);
  REQUIRE_EQ(liveness.live_in[2U][1U], 1U);
  verify_liveness_oracle(graph, 2U, blocks, liveness);
}

TEST_CASE(pruned_ssa_validation_matches_scalar_domain) {
  {
    Graph graph(2U, false);
    std::vector<std::vector<SsaInputInstruction>> blocks(2U);
    REQUIRE_THROWS_AS(analyze_ssa_liveness(graph, 0U, 1U, blocks),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(construct_pruned_ssa(graph, 0U, 1U, blocks),
                      std::invalid_argument);
  }
  {
    Graph graph(2U, true);
    graph.add_edge(1U, 0U);
    std::vector<std::vector<SsaInputInstruction>> blocks(2U);
    REQUIRE_THROWS_AS(construct_pruned_ssa(graph, 0U, 1U, blocks),
                      std::invalid_argument);
  }
  {
    Graph graph(2U, true);
    std::vector<std::vector<SsaInputInstruction>> blocks(2U);
    blocks[1U].push_back(SsaInputInstruction{{0U}, std::nullopt});
    REQUIRE_THROWS_AS(analyze_ssa_liveness(graph, 0U, 1U, blocks),
                      std::invalid_argument);
  }
  {
    Graph graph(1U, true);
    std::vector<std::vector<SsaInputInstruction>> blocks(1U);
    blocks[0U].push_back(SsaInputInstruction{{1U}, std::nullopt});
    REQUIRE_THROWS_AS(construct_pruned_ssa(graph, 0U, 1U, blocks),
                      std::out_of_range);
  }
}

void verify_pruned_random_dag(
    const Graph& graph, std::size_t variable_count,
    const std::vector<std::vector<SsaInputInstruction>>& input,
    const SsaLiveness& liveness, const SsaProgram& program) {
  const std::size_t vertex_count = graph.vertex_count();
  const auto predecessors = unique_predecessors(graph);
  verify_liveness_oracle(graph, variable_count, input, liveness);

  std::vector<std::vector<Symbol>> outgoing(
      vertex_count, std::vector<Symbol>(variable_count));
  std::vector<std::vector<std::optional<Symbol>>> expected_phi(
      vertex_count, std::vector<std::optional<Symbol>>(variable_count));
  std::vector<std::vector<std::vector<Symbol>>> expected_uses(vertex_count);

  for (Vertex block = 0U; block < vertex_count; ++block) {
    std::vector<Symbol> current(variable_count);
    for (Variable variable = 0U; variable < variable_count; ++variable) {
      if (block == 0U) {
        current[variable] =
            Symbol{SymbolKind::initial, 0U, 0U, variable};
      } else {
        REQUIRE(!predecessors[block].empty());
        const Symbol first = outgoing[predecessors[block][0U]][variable];
        bool same = true;
        for (const Vertex predecessor : predecessors[block]) {
          if (!(outgoing[predecessor][variable] == first)) {
            same = false;
            break;
          }
        }
        if (!same && liveness.live_in[block][variable] != 0U) {
          current[variable] =
              Symbol{SymbolKind::phi, block, 0U, variable};
          expected_phi[block][variable] = current[variable];
        } else {
          current[variable] = first;
        }
      }
    }

    expected_uses[block].resize(input[block].size());
    for (std::size_t instruction = 0U; instruction < input[block].size();
         ++instruction) {
      for (const Variable variable : input[block][instruction].uses) {
        expected_uses[block][instruction].push_back(current[variable]);
      }
      if (input[block][instruction].definition.has_value()) {
        const Variable variable = *input[block][instruction].definition;
        current[variable] = Symbol{SymbolKind::definition, block, instruction,
                                   variable};
      }
    }
    outgoing[block] = current;
  }

  std::map<std::pair<Variable, std::size_t>, Symbol> identity;
  for (Variable variable = 0U; variable < variable_count; ++variable) {
    REQUIRE(identity
                .emplace(std::pair{variable, 0U},
                         Symbol{SymbolKind::initial, 0U, 0U, variable})
                .second);
  }
  for (Vertex block = 0U; block < vertex_count; ++block) {
    for (const SsaPhi& phi : program.blocks[block].phis) {
      REQUIRE(identity
                  .emplace(std::pair{phi.result.variable, phi.result.version},
                           Symbol{SymbolKind::phi, block, 0U, phi.variable})
                  .second);
    }
    for (std::size_t instruction = 0U;
         instruction < program.blocks[block].instructions.size();
         ++instruction) {
      const auto& renamed = program.blocks[block].instructions[instruction];
      if (renamed.definition.has_value()) {
        const SsaValue definition = *renamed.definition;
        REQUIRE(identity
                    .emplace(
                        std::pair{definition.variable, definition.version},
                        Symbol{SymbolKind::definition, block, instruction,
                               definition.variable})
                    .second);
      }
    }
  }

  const auto symbol_of = [&](SsaValue value) {
    const auto iterator = identity.find({value.variable, value.version});
    REQUIRE(iterator != identity.end());
    return iterator->second;
  };

  for (Vertex block = 0U; block < vertex_count; ++block) {
    std::set<Variable> actual_phi_variables;
    for (const SsaPhi& phi : program.blocks[block].phis) {
      actual_phi_variables.insert(phi.variable);
      REQUIRE(liveness.live_in[block][phi.variable] != 0U);
      REQUIRE(expected_phi[block][phi.variable].has_value());
      REQUIRE(symbol_of(phi.result) == *expected_phi[block][phi.variable]);
      REQUIRE_EQ(phi.incoming.size(), predecessors[block].size());
      for (std::size_t index = 0U; index < phi.incoming.size(); ++index) {
        REQUIRE_EQ(phi.incoming[index].predecessor,
                   predecessors[block][index]);
        REQUIRE(symbol_of(phi.incoming[index].value) ==
                outgoing[predecessors[block][index]][phi.variable]);
      }
    }
    for (Variable variable = 0U; variable < variable_count; ++variable) {
      REQUIRE(actual_phi_variables.contains(variable) ==
              expected_phi[block][variable].has_value());
    }

    REQUIRE_EQ(program.blocks[block].instructions.size(), input[block].size());
    for (std::size_t instruction = 0U; instruction < input[block].size();
         ++instruction) {
      const auto& renamed = program.blocks[block].instructions[instruction];
      REQUIRE_EQ(renamed.uses.size(), expected_uses[block][instruction].size());
      for (std::size_t use = 0U; use < renamed.uses.size(); ++use) {
        REQUIRE(symbol_of(renamed.uses[use]) ==
                expected_uses[block][instruction][use]);
      }
      if (input[block][instruction].definition.has_value()) {
        REQUIRE(renamed.definition.has_value());
        const Symbol expected{SymbolKind::definition, block, instruction,
                              *input[block][instruction].definition};
        REQUIRE(symbol_of(*renamed.definition) == expected);
      } else {
        REQUIRE(!renamed.definition.has_value());
      }
    }
  }
}

TEST_CASE(pruned_ssa_randomized_dag_liveness_and_reaching_definition_oracles) {
  std::mt19937_64 random(0x5052554E45445353ULL);
  for (int trial = 0; trial < 350; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(random() % 9U);
    const std::size_t variable_count =
        1U + static_cast<std::size_t>(random() % 4U);
    Graph graph(vertex_count, true);

    for (Vertex vertex = 1U; vertex < vertex_count; ++vertex) {
      const Vertex predecessor = static_cast<Vertex>(random() % vertex);
      graph.add_edge(predecessor, vertex, static_cast<Weight>(random()));
      if ((random() % 5U) == 0U) {
        graph.add_edge(predecessor, vertex, static_cast<Weight>(random()));
      }
    }
    for (Vertex source = 0U; source < vertex_count; ++source) {
      for (Vertex target = source + 1U; target < vertex_count; ++target) {
        if ((random() % 5U) == 0U) {
          graph.add_edge(source, target, static_cast<Weight>(random()));
        }
      }
    }

    std::vector<std::vector<SsaInputInstruction>> input(vertex_count);
    for (Vertex block = 0U; block < vertex_count; ++block) {
      const std::size_t instruction_count =
          static_cast<std::size_t>(random() % 4U);
      for (std::size_t instruction = 0U; instruction < instruction_count;
           ++instruction) {
        SsaInputInstruction event;
        const std::size_t use_count =
            static_cast<std::size_t>(random() % 3U);
        for (std::size_t use = 0U; use < use_count; ++use) {
          event.uses.push_back(static_cast<Variable>(random() % variable_count));
        }
        if ((random() % 2U) == 0U) {
          event.definition = static_cast<Variable>(random() % variable_count);
        }
        input[block].push_back(std::move(event));
      }
    }

    const SsaLiveness liveness =
        analyze_ssa_liveness(graph, 0U, variable_count, input);
    const SsaProgram program =
        construct_pruned_ssa(graph, 0U, variable_count, input);
    verify_pruned_random_dag(graph, variable_count, input, liveness, program);
    REQUIRE(construct_pruned_ssa(graph, 0U, variable_count, input) == program);
  }
}

}  // namespace
