#include "algorithms/graphs/ssa_construction.hpp"

#include "algorithms/graphs/dominance_frontier.hpp"
#include "algorithms/graphs/dominator_tree.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

std::size_t next_version(std::vector<std::size_t>& counters, Variable variable) {
  if (counters[variable] == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error("SSA version counter overflow");
  }
  ++counters[variable];
  return counters[variable];
}

void validate_variable(Variable variable, std::size_t variable_count) {
  if (variable >= variable_count) {
    throw std::out_of_range("SSA variable id out of range");
  }
}

}  // namespace

SsaProgram construct_ssa(
    const Graph& graph, Vertex start, std::size_t variable_count,
    const std::vector<std::vector<SsaInputInstruction>>& input_blocks) {
  graph.validate_vertex(start);
  if (!graph.directed()) {
    throw std::invalid_argument("SSA construction requires a directed graph");
  }
  if (input_blocks.size() != graph.vertex_count()) {
    throw std::invalid_argument("SSA block count must match graph vertex count");
  }

  for (const auto& block : input_blocks) {
    for (const SsaInputInstruction& instruction : block) {
      for (const Variable variable : instruction.uses) {
        validate_variable(variable, variable_count);
      }
      if (instruction.definition.has_value()) {
        validate_variable(*instruction.definition, variable_count);
      }
    }
  }

  for (Vertex source = 0U; source < graph.vertex_count(); ++source) {
    for (const Edge& edge : graph.neighbors(source)) {
      if (edge.to == start) {
        throw std::invalid_argument("SSA entry block must be predecessor-free");
      }
    }
  }

  const DominatorTree dominators(graph, start);
  const DominanceFrontierIndex frontiers(graph, start);

  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    if (!dominators.reachable(block) && !input_blocks[block].empty()) {
      throw std::invalid_argument(
          "unreachable SSA blocks must not contain input instructions");
    }
  }

  SsaProgram program;
  program.start = start;
  program.variable_count = variable_count;
  program.blocks.resize(graph.vertex_count());
  program.initial_values.reserve(variable_count);
  for (Variable variable = 0U; variable < variable_count; ++variable) {
    program.initial_values.push_back(SsaValue{variable, 0U});
  }
  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    program.blocks[block].reachable = dominators.reachable(block);
  }

  for (Variable variable = 0U; variable < variable_count; ++variable) {
    std::vector<Vertex> definitions{start};
    for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
      if (!dominators.reachable(block)) {
        continue;
      }
      bool defines = false;
      for (const SsaInputInstruction& instruction : input_blocks[block]) {
        if (instruction.definition == variable) {
          defines = true;
          break;
        }
      }
      if (defines && block != start) {
        definitions.push_back(block);
      }
    }

    const std::vector<Vertex> placement =
        frontiers.iterated_frontier(definitions);
    for (const Vertex block : placement) {
      if (block == start) {
        throw std::logic_error("predecessor-free SSA entry acquired a phi");
      }
      program.blocks[block].phis.push_back(
          SsaPhi{variable, SsaValue{variable, 0U}, {}});
    }
  }
  for (SsaBlock& block : program.blocks) {
    std::sort(block.phis.begin(), block.phis.end(),
              [](const SsaPhi& left, const SsaPhi& right) {
                return left.variable < right.variable;
              });
  }

  std::vector<std::vector<Vertex>> predecessors(graph.vertex_count());
  for (Vertex source = 0U; source < graph.vertex_count(); ++source) {
    if (!dominators.reachable(source)) {
      continue;
    }
    for (const Edge& edge : graph.neighbors(source)) {
      if (dominators.reachable(edge.to)) {
        predecessors[edge.to].push_back(source);
      }
    }
  }
  for (auto& list : predecessors) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }

  std::vector<std::size_t> counters(variable_count, 0U);
  std::vector<std::vector<std::size_t>> stacks(variable_count);
  for (Variable variable = 0U; variable < variable_count; ++variable) {
    stacks[variable].push_back(0U);
  }

  auto add_successor_incoming = [&](Vertex source) {
    std::vector<Vertex> targets;
    for (const Edge& edge : graph.neighbors(source)) {
      if (dominators.reachable(edge.to)) {
        targets.push_back(edge.to);
      }
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    for (const Vertex target : targets) {
      for (SsaPhi& phi : program.blocks[target].phis) {
        phi.incoming.push_back(
            SsaPhiIncoming{source,
                           SsaValue{phi.variable, stacks[phi.variable].back()}});
      }
    }
  };

  const auto& tree_children = dominators.tree_children();
  struct RenameFrame {
    Vertex block;
    bool entered = false;
    std::vector<Vertex> children;
    std::size_t next_child = 0U;
    std::vector<Variable> pushed_variables;
  };

  std::vector<RenameFrame> rename_stack;
  rename_stack.push_back(RenameFrame{start, false, {}, 0U, {}});
  while (!rename_stack.empty()) {
    RenameFrame& frame = rename_stack.back();
    if (!frame.entered) {
      for (SsaPhi& phi : program.blocks[frame.block].phis) {
        const std::size_t version = next_version(counters, phi.variable);
        phi.result = SsaValue{phi.variable, version};
        stacks[phi.variable].push_back(version);
        frame.pushed_variables.push_back(phi.variable);
      }

      auto& output = program.blocks[frame.block].instructions;
      output.reserve(input_blocks[frame.block].size());
      for (const SsaInputInstruction& input : input_blocks[frame.block]) {
        SsaInstruction renamed;
        renamed.uses.reserve(input.uses.size());
        for (const Variable variable : input.uses) {
          renamed.uses.push_back(SsaValue{variable, stacks[variable].back()});
        }
        if (input.definition.has_value()) {
          const Variable variable = *input.definition;
          const std::size_t version = next_version(counters, variable);
          renamed.definition = SsaValue{variable, version};
          stacks[variable].push_back(version);
          frame.pushed_variables.push_back(variable);
        }
        output.push_back(std::move(renamed));
      }

      add_successor_incoming(frame.block);
      frame.children = tree_children[frame.block];
      std::sort(frame.children.begin(), frame.children.end());
      frame.entered = true;
    }

    if (frame.next_child < frame.children.size()) {
      const Vertex child = frame.children[frame.next_child];
      ++frame.next_child;
      rename_stack.push_back(RenameFrame{child, false, {}, 0U, {}});
      continue;
    }

    for (auto iterator = frame.pushed_variables.rbegin();
         iterator != frame.pushed_variables.rend(); ++iterator) {
      stacks[*iterator].pop_back();
    }
    rename_stack.pop_back();
  }

  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    for (SsaPhi& phi : program.blocks[block].phis) {
      std::sort(phi.incoming.begin(), phi.incoming.end(),
                [](const SsaPhiIncoming& left, const SsaPhiIncoming& right) {
                  return left.predecessor < right.predecessor;
                });
      if (phi.incoming.size() != predecessors[block].size()) {
        throw std::logic_error("SSA phi incoming predecessor mismatch");
      }
      for (std::size_t index = 0U; index < phi.incoming.size(); ++index) {
        if (phi.incoming[index].predecessor != predecessors[block][index]) {
          throw std::logic_error("SSA phi incoming order mismatch");
        }
      }
    }
  }

  return program;
}

}  // namespace algorithms::graphs
