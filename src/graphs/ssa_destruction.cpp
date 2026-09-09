#include "algorithms/graphs/ssa_destruction.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

[[nodiscard]] bool value_less(const SsaValue& first, const SsaValue& second) {
  return std::tie(first.variable, first.version) <
         std::tie(second.variable, second.version);
}

[[nodiscard]] bool location_equal(const SsaCopyLocation& first,
                                  const SsaCopyLocation& second) {
  return first == second;
}

[[nodiscard]] SsaCopyLocation value_location(const SsaValue value) {
  return SsaCopyLocation::from_value(value);
}

struct LogicalSitePlan {
  Vertex predecessor{0U};
  Vertex successor{0U};
  SsaCopyPlacement placement{SsaCopyPlacement::predecessor_exit};
  std::vector<SsaParallelCopy> copies;
  SsaCopySchedule local_schedule;
  std::vector<Vertex> execution_blocks;
};

[[nodiscard]] std::vector<unsigned char> reachable_from(const Graph& graph,
                                                        const Vertex start) {
  graph.validate_vertex(start);
  std::vector<unsigned char> reachable(graph.vertex_count(), 0U);
  std::queue<Vertex> pending;
  reachable[start] = 1U;
  pending.push(start);
  while (!pending.empty()) {
    const Vertex block = pending.front();
    pending.pop();
    for (const Edge& edge : graph.neighbors(block)) {
      if (reachable[edge.to] == 0U) {
        reachable[edge.to] = 1U;
        pending.push(edge.to);
      }
    }
  }
  return reachable;
}

[[nodiscard]] std::vector<std::vector<Vertex>> unique_reachable_successors(
    const Graph& graph, const std::vector<unsigned char>& reachable) {
  std::vector<std::vector<Vertex>> successors(graph.vertex_count());
  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    if (reachable[block] == 0U) {
      continue;
    }
    for (const Edge& edge : graph.neighbors(block)) {
      if (reachable[edge.to] != 0U) {
        successors[block].push_back(edge.to);
      }
    }
    std::sort(successors[block].begin(), successors[block].end());
    successors[block].erase(
        std::unique(successors[block].begin(), successors[block].end()),
        successors[block].end());
  }
  return successors;
}

[[nodiscard]] std::vector<std::vector<Vertex>> unique_reachable_predecessors(
    const std::vector<std::vector<Vertex>>& successors) {
  std::vector<std::vector<Vertex>> predecessors(successors.size());
  for (Vertex block = 0U; block < successors.size(); ++block) {
    for (const Vertex successor : successors[block]) {
      predecessors[successor].push_back(block);
    }
  }
  for (std::vector<Vertex>& list : predecessors) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }
  return predecessors;
}

void validate_value(const SsaValue value, const std::size_t variable_count) {
  if (value.variable >= variable_count) {
    throw std::invalid_argument("SSA value variable out of range");
  }
}

void validate_program(
    const Graph& graph, const SsaProgram& program,
    const std::vector<unsigned char>& reachable,
    const std::vector<std::vector<Vertex>>& predecessors) {
  if (!graph.directed()) {
    throw std::invalid_argument("SSA destruction requires a directed CFG");
  }
  if (program.blocks.size() != graph.vertex_count()) {
    throw std::invalid_argument("SSA block count does not match CFG");
  }
  graph.validate_vertex(program.start);
  if (!predecessors[program.start].empty()) {
    throw std::invalid_argument("SSA entry block must be predecessor-free");
  }
  if (program.initial_values.size() != program.variable_count) {
    throw std::invalid_argument("SSA initial-value count does not match variables");
  }
  for (Variable variable = 0U; variable < program.variable_count; ++variable) {
    if (program.initial_values[variable] != SsaValue{variable, 0U}) {
      throw std::invalid_argument("SSA initial values must be version zero");
    }
  }

  std::set<std::pair<Variable, std::size_t>> definitions;
  for (const SsaValue initial : program.initial_values) {
    definitions.emplace(initial.variable, initial.version);
  }

  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    const SsaBlock& ssa_block = program.blocks[block];
    const bool expected_reachable = reachable[block] != 0U;
    if (ssa_block.reachable != expected_reachable) {
      throw std::invalid_argument("SSA reachable flags do not match CFG");
    }
    if (!expected_reachable &&
        (!ssa_block.phis.empty() || !ssa_block.instructions.empty())) {
      throw std::invalid_argument("unreachable SSA blocks must be empty");
    }

    std::set<Variable> phi_variables;
    for (const SsaPhi& phi : ssa_block.phis) {
      if (phi.variable >= program.variable_count ||
          phi.result.variable != phi.variable) {
        throw std::invalid_argument("malformed SSA phi result");
      }
      if (!phi_variables.insert(phi.variable).second) {
        throw std::invalid_argument("duplicate SSA phi variable in block");
      }
      if (!definitions
               .emplace(phi.result.variable, phi.result.version)
               .second) {
        throw std::invalid_argument("SSA value has multiple definitions");
      }

      std::vector<Vertex> incoming_predecessors;
      incoming_predecessors.reserve(phi.incoming.size());
      for (const SsaPhiIncoming& incoming : phi.incoming) {
        if (incoming.value.variable != phi.variable) {
          throw std::invalid_argument("SSA phi incoming variable mismatch");
        }
        incoming_predecessors.push_back(incoming.predecessor);
      }
      std::sort(incoming_predecessors.begin(), incoming_predecessors.end());
      if (std::adjacent_find(incoming_predecessors.begin(),
                             incoming_predecessors.end()) !=
          incoming_predecessors.end()) {
        throw std::invalid_argument("duplicate SSA phi predecessor");
      }
      if (incoming_predecessors != predecessors[block]) {
        throw std::invalid_argument(
            "SSA phi incoming predecessors do not match CFG");
      }
    }

    for (const SsaInstruction& instruction : ssa_block.instructions) {
      for (const SsaValue use : instruction.uses) {
        validate_value(use, program.variable_count);
      }
      if (instruction.definition.has_value()) {
        validate_value(*instruction.definition, program.variable_count);
        if (!definitions
                 .emplace(instruction.definition->variable,
                          instruction.definition->version)
                 .second) {
          throw std::invalid_argument("SSA value has multiple definitions");
        }
      }
    }
  }

  for (const SsaBlock& block : program.blocks) {
    for (const SsaPhi& phi : block.phis) {
      for (const SsaPhiIncoming& incoming : phi.incoming) {
        if (!definitions.contains(
                {incoming.value.variable, incoming.value.version})) {
          throw std::invalid_argument("SSA phi incoming value is undefined");
        }
      }
    }
    for (const SsaInstruction& instruction : block.instructions) {
      for (const SsaValue use : instruction.uses) {
        if (!definitions.contains({use.variable, use.version})) {
          throw std::invalid_argument("SSA instruction use is undefined");
        }
      }
    }
  }
}

[[nodiscard]] const SsaPhiIncoming& incoming_for(
    const SsaPhi& phi, const Vertex predecessor) {
  const auto found = std::find_if(
      phi.incoming.begin(), phi.incoming.end(),
      [predecessor](const SsaPhiIncoming& incoming) {
        return incoming.predecessor == predecessor;
      });
  if (found == phi.incoming.end()) {
    throw std::logic_error("validated SSA phi is missing predecessor");
  }
  return *found;
}

[[nodiscard]] std::vector<SsaParallelCopy> copies_for_edge(
    const SsaBlock& successor_block, const Vertex predecessor) {
  std::vector<SsaParallelCopy> copies;
  copies.reserve(successor_block.phis.size());
  for (const SsaPhi& phi : successor_block.phis) {
    const SsaPhiIncoming& incoming = incoming_for(phi, predecessor);
    if (incoming.value != phi.result) {
      copies.push_back(SsaParallelCopy{phi.result, incoming.value});
    }
  }
  std::sort(copies.begin(), copies.end(),
            [](const SsaParallelCopy& first, const SsaParallelCopy& second) {
              if (first.destination != second.destination) {
                return value_less(first.destination, second.destination);
              }
              return value_less(first.source, second.source);
            });
  return copies;
}

void add_global_offset(std::vector<SsaScheduledMove>& schedule,
                       const std::size_t offset) {
  for (SsaScheduledMove& move : schedule) {
    if (move.destination.kind == SsaCopyLocationKind::temporary) {
      move.destination.temporary += offset;
    }
    if (move.source.kind == SsaCopyLocationKind::temporary) {
      move.source.temporary += offset;
    }
  }
}

}  // namespace

SsaCopyLocation SsaCopyLocation::from_value(const SsaValue value) {
  return SsaCopyLocation{SsaCopyLocationKind::value, value, 0U};
}

SsaCopyLocation SsaCopyLocation::from_temporary(
    const std::size_t temporary) {
  return SsaCopyLocation{SsaCopyLocationKind::temporary, SsaValue{0U, 0U},
                         temporary};
}

SsaCopySchedule schedule_parallel_copies(
    const std::vector<SsaParallelCopy>& copies) {
  struct Pending {
    SsaCopyLocation destination;
    SsaCopyLocation source;
  };

  std::vector<Pending> pending;
  pending.reserve(copies.size());
  std::set<std::pair<Variable, std::size_t>> destinations;
  for (const SsaParallelCopy& copy : copies) {
    if (!destinations
             .emplace(copy.destination.variable, copy.destination.version)
             .second) {
      throw std::invalid_argument("parallel copies have duplicate destination");
    }
    if (copy.destination == copy.source) {
      continue;
    }
    pending.push_back(
        Pending{value_location(copy.destination), value_location(copy.source)});
  }
  std::sort(pending.begin(), pending.end(),
            [](const Pending& first, const Pending& second) {
              return value_less(first.destination.value,
                                second.destination.value);
            });

  SsaCopySchedule result;
  while (!pending.empty()) {
    auto ready = pending.end();
    for (auto candidate = pending.begin(); candidate != pending.end();
         ++candidate) {
      const bool destination_still_needed = std::any_of(
          pending.begin(), pending.end(),
          [&candidate](const Pending& other) {
            return location_equal(other.source, candidate->destination);
          });
      if (!destination_still_needed) {
        ready = candidate;
        break;
      }
    }

    if (ready != pending.end()) {
      result.moves.push_back(
          SsaScheduledMove{ready->destination, ready->source});
      pending.erase(ready);
      continue;
    }

    // Every remaining destination is still needed as a source, so at least one
    // cycle exists. Save the lexicographically smallest destination and replace
    // all uses of its old value by a fresh temporary.
    const SsaCopyLocation saved = pending.front().destination;
    const SsaCopyLocation temporary =
        SsaCopyLocation::from_temporary(result.temporary_count);
    ++result.temporary_count;
    result.moves.push_back(SsaScheduledMove{temporary, saved});
    for (Pending& copy : pending) {
      if (location_equal(copy.source, saved)) {
        copy.source = temporary;
      }
    }
  }
  return result;
}

OutOfSsaProgram destroy_ssa(const Graph& graph, const SsaProgram& program) {
  if (!graph.directed()) {
    throw std::invalid_argument("SSA destruction requires a directed CFG");
  }
  const std::vector<unsigned char> reachable =
      reachable_from(graph, program.start);
  const std::vector<std::vector<Vertex>> successors =
      unique_reachable_successors(graph, reachable);
  const std::vector<std::vector<Vertex>> predecessors =
      unique_reachable_predecessors(successors);
  validate_program(graph, program, reachable, predecessors);

  std::vector<LogicalSitePlan> plans;
  for (Vertex successor = 0U; successor < graph.vertex_count(); ++successor) {
    if (program.blocks[successor].phis.empty()) {
      continue;
    }
    for (const Vertex predecessor : predecessors[successor]) {
      std::vector<SsaParallelCopy> copies =
          copies_for_edge(program.blocks[successor], predecessor);
      if (copies.empty()) {
        continue;
      }
      SsaCopyPlacement placement = SsaCopyPlacement::split_blocks;
      if (successors[predecessor].size() == 1U) {
        placement = SsaCopyPlacement::predecessor_exit;
      } else if (predecessors[successor].size() == 1U) {
        placement = SsaCopyPlacement::successor_entry;
      }
      plans.push_back(LogicalSitePlan{predecessor, successor, placement,
                                      std::move(copies), {}, {}});
    }
  }
  std::sort(plans.begin(), plans.end(),
            [](const LogicalSitePlan& first, const LogicalSitePlan& second) {
              return std::tie(first.predecessor, first.successor) <
                     std::tie(second.predecessor, second.successor);
            });

  std::size_t split_count = 0U;
  for (LogicalSitePlan& plan : plans) {
    plan.local_schedule = schedule_parallel_copies(plan.copies);
    if (plan.placement == SsaCopyPlacement::split_blocks) {
      for (const Edge& edge : graph.neighbors(plan.predecessor)) {
        if (edge.to == plan.successor) {
          ++split_count;
        }
      }
    }
  }

  const std::size_t original_count = graph.vertex_count();
  if (split_count >
      std::numeric_limits<std::size_t>::max() - original_count) {
    throw std::length_error("SSA destruction split-block count overflows");
  }
  const std::size_t lowered_count = original_count + split_count;

  std::vector<std::vector<std::optional<Vertex>>> split_for_edge(
      original_count);
  for (Vertex block = 0U; block < original_count; ++block) {
    split_for_edge[block].resize(graph.neighbors(block).size());
  }

  Vertex next_split = original_count;
  for (LogicalSitePlan& plan : plans) {
    if (plan.placement == SsaCopyPlacement::predecessor_exit) {
      plan.execution_blocks.push_back(plan.predecessor);
    } else if (plan.placement == SsaCopyPlacement::successor_entry) {
      plan.execution_blocks.push_back(plan.successor);
    } else {
      const auto& edges = graph.neighbors(plan.predecessor);
      for (std::size_t edge_index = 0U; edge_index < edges.size();
           ++edge_index) {
        if (edges[edge_index].to == plan.successor) {
          plan.execution_blocks.push_back(next_split);
          split_for_edge[plan.predecessor][edge_index] = next_split;
          ++next_split;
        }
      }
    }
  }

  Graph lowered_graph(lowered_count, true);
  for (Vertex block = 0U; block < original_count; ++block) {
    const auto& edges = graph.neighbors(block);
    for (std::size_t edge_index = 0U; edge_index < edges.size();
         ++edge_index) {
      const std::optional<Vertex> split = split_for_edge[block][edge_index];
      if (!split.has_value()) {
        lowered_graph.add_edge(block, edges[edge_index].to,
                               edges[edge_index].weight);
      } else {
        lowered_graph.add_edge(block, *split, edges[edge_index].weight);
        lowered_graph.add_edge(*split, edges[edge_index].to, 0);
      }
    }
  }

  std::vector<SsaLoweredBlock> lowered_blocks(lowered_count);
  for (Vertex block = 0U; block < original_count; ++block) {
    lowered_blocks[block].reachable = program.blocks[block].reachable;
    lowered_blocks[block].original_block = block;
    lowered_blocks[block].instructions = program.blocks[block].instructions;
  }
  for (Vertex block = original_count; block < lowered_count; ++block) {
    lowered_blocks[block].reachable = true;
  }

  std::vector<SsaEdgeLowering> edge_lowerings;
  edge_lowerings.reserve(plans.size());
  std::size_t temporary_count = 0U;
  for (LogicalSitePlan& plan : plans) {
    std::vector<SsaScheduledMove> schedule = plan.local_schedule.moves;
    if (plan.local_schedule.temporary_count >
        std::numeric_limits<std::size_t>::max() - temporary_count) {
      throw std::length_error("SSA destruction temporary count overflows");
    }
    add_global_offset(schedule, temporary_count);
    temporary_count += plan.local_schedule.temporary_count;

    if (plan.placement == SsaCopyPlacement::predecessor_exit) {
      lowered_blocks[plan.predecessor].exit_moves = schedule;
    } else if (plan.placement == SsaCopyPlacement::successor_entry) {
      lowered_blocks[plan.successor].entry_moves = schedule;
    } else {
      for (const Vertex split : plan.execution_blocks) {
        lowered_blocks[split].entry_moves = schedule;
      }
    }

    edge_lowerings.push_back(SsaEdgeLowering{
        plan.predecessor, plan.successor, plan.placement,
        plan.execution_blocks, plan.copies, std::move(schedule)});
  }

  return OutOfSsaProgram{program.start,
                         program.variable_count,
                         program.initial_values,
                         std::move(lowered_graph),
                         std::move(lowered_blocks),
                         std::move(edge_lowerings),
                         temporary_count};
}

}  // namespace algorithms::graphs
