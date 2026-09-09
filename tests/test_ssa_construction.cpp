#include "algorithms/graphs/ssa_construction.hpp"

#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

TEST_CASE(ssa_diamond_phi_and_version_zero) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  graph.add_edge(1U, 3U);
  graph.add_edge(2U, 3U);

  std::vector<std::vector<SsaInputInstruction>> blocks(4U);
  blocks[1U].push_back(SsaInputInstruction{{}, Variable{0U}});
  blocks[3U].push_back(SsaInputInstruction{{0U}, std::nullopt});

  const SsaProgram program = construct_ssa(graph, 0U, 1U, blocks);
  REQUIRE_EQ(program.blocks[3U].phis.size(), 1U);
  const SsaPhi& phi = program.blocks[3U].phis[0U];
  REQUIRE_EQ(phi.variable, 0U);
  REQUIRE_EQ(phi.incoming.size(), 2U);
  REQUIRE_EQ(phi.incoming[0U].predecessor, 1U);
  REQUIRE_EQ(phi.incoming[1U].predecessor, 2U);
  REQUIRE(phi.incoming[0U].value.version != 0U);
  REQUIRE_EQ(phi.incoming[1U].value.version, 0U);
  REQUIRE(program.blocks[3U].instructions[0U].uses[0U] == phi.result);
  REQUIRE(construct_ssa(graph, 0U, 1U, blocks) == program);
}

TEST_CASE(ssa_loop_backedge_and_parallel_predecessors) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U, 1);
  graph.add_edge(0U, 1U, 9);
  graph.add_edge(1U, 2U);
  graph.add_edge(2U, 1U);
  graph.add_edge(1U, 3U);

  std::vector<std::vector<SsaInputInstruction>> blocks(4U);
  blocks[1U].push_back(SsaInputInstruction{{0U}, std::nullopt});
  blocks[2U].push_back(SsaInputInstruction{{0U}, Variable{0U}});

  const SsaProgram program = construct_ssa(graph, 0U, 1U, blocks);
  REQUIRE_EQ(program.blocks[1U].phis.size(), 1U);
  const SsaPhi& phi = program.blocks[1U].phis[0U];
  REQUIRE_EQ(phi.incoming.size(), 2U);
  REQUIRE_EQ(phi.incoming[0U].predecessor, 0U);
  REQUIRE_EQ(phi.incoming[1U].predecessor, 2U);
  REQUIRE_EQ(phi.incoming[0U].value.version, 0U);
  REQUIRE(program.blocks[1U].instructions[0U].uses[0U] == phi.result);
  REQUIRE(program.blocks[2U].instructions[0U].uses[0U] == phi.result);
  REQUIRE(phi.incoming[1U].value ==
          *program.blocks[2U].instructions[0U].definition);
}

TEST_CASE(ssa_validation_contract) {
  {
    Graph graph(2U, false);
    std::vector<std::vector<SsaInputInstruction>> blocks(2U);
    REQUIRE_THROWS_AS(construct_ssa(graph, 0U, 1U, blocks),
                      std::invalid_argument);
  }
  {
    Graph graph(2U, true);
    graph.add_edge(1U, 0U);
    std::vector<std::vector<SsaInputInstruction>> blocks(2U);
    REQUIRE_THROWS_AS(construct_ssa(graph, 0U, 1U, blocks),
                      std::invalid_argument);
  }
  {
    Graph graph(2U, true);
    std::vector<std::vector<SsaInputInstruction>> blocks(2U);
    blocks[1U].push_back(SsaInputInstruction{{0U}, std::nullopt});
    REQUIRE_THROWS_AS(construct_ssa(graph, 0U, 1U, blocks),
                      std::invalid_argument);
  }
  {
    Graph graph(1U, true);
    std::vector<std::vector<SsaInputInstruction>> blocks(1U);
    blocks[0U].push_back(SsaInputInstruction{{1U}, std::nullopt});
    REQUIRE_THROWS_AS(construct_ssa(graph, 0U, 1U, blocks),
                      std::out_of_range);
  }
  {
    Graph graph(1U, true);
    std::vector<std::vector<SsaInputInstruction>> blocks;
    REQUIRE_THROWS_AS(construct_ssa(graph, 0U, 1U, blocks),
                      std::invalid_argument);
  }
}

void verify_random_dag(
    const Graph& graph, std::size_t variable_count,
    const std::vector<std::vector<SsaInputInstruction>>& input,
    const SsaProgram& program) {
  const std::size_t vertex_count = graph.vertex_count();
  std::vector<std::vector<Vertex>> predecessors(vertex_count);
  for (Vertex source = 0U; source < vertex_count; ++source) {
    for (const Edge& edge : graph.neighbors(source)) {
      predecessors[edge.to].push_back(source);
    }
  }
  for (auto& list : predecessors) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }

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
        if (same) {
          current[variable] = first;
        } else {
          current[variable] =
              Symbol{SymbolKind::phi, block, 0U, variable};
          expected_phi[block][variable] = current[variable];
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

TEST_CASE(ssa_randomized_dag_reaching_definition_differential) {
  std::mt19937_64 random(0x53534145ULL);
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

    const SsaProgram program =
        construct_ssa(graph, 0U, variable_count, input);
    verify_random_dag(graph, variable_count, input, program);
    REQUIRE(construct_ssa(graph, 0U, variable_count, input) == program);
  }
}

}  // namespace
