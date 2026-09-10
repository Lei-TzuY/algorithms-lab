#include "algorithms/graphs/register_allocation.hpp"

#include <algorithm>
#include <cstddef>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

struct LocationLess {
  bool operator()(const SsaCopyLocation& first,
                  const SsaCopyLocation& second) const {
    if (first.kind != second.kind) {
      return first.kind == SsaCopyLocationKind::value;
    }
    if (first.kind == SsaCopyLocationKind::value) {
      return std::tie(first.value.variable, first.value.version) <
             std::tie(second.value.variable, second.value.version);
    }
    return first.temporary < second.temporary;
  }
};

struct Operation {
  PhiFreeOperationKind kind{PhiFreeOperationKind::instruction};
  std::size_t index{0U};
  std::vector<SsaCopyLocation> uses;
  std::vector<SsaCopyLocation> definitions;
};

using Bits = std::vector<unsigned char>;

[[nodiscard]] bool same_bits(const Bits& first, const Bits& second) {
  return first == second;
}

void insert_location(std::vector<SsaCopyLocation>& locations,
                     const SsaCopyLocation location,
                     const std::size_t variable_count,
                     const std::size_t temporary_count) {
  if (location.kind == SsaCopyLocationKind::value) {
    if (location.value.variable >= variable_count) {
      throw std::invalid_argument(
          "register-allocation SSA value variable out of range");
    }
  } else if (location.temporary >= temporary_count) {
    throw std::invalid_argument("register-allocation temporary out of range");
  }
  locations.push_back(location);
}

[[nodiscard]] bool has_cfg_edge(const Graph& graph, const Vertex from,
                                const Vertex to) {
  if (to >= graph.vertex_count()) {
    return false;
  }
  const auto& neighbors = graph.neighbors(from);
  return std::any_of(neighbors.begin(), neighbors.end(),
                     [to](const Edge& edge) { return edge.to == to; });
}

void validate_control_target(const Graph& graph, const Vertex block,
                             const SsaLoweredControlTarget& target) {
  if (target.logical_successor >= graph.vertex_count() ||
      !has_cfg_edge(graph, block, target.execution_successor)) {
    throw std::invalid_argument(
        "register-allocation control target is outside lowered CFG");
  }
}

[[nodiscard]] std::optional<SsaCopyLocation> validate_control(
    const OutOfSsaProgram& program, const Vertex block) {
  const SsaLoweredBlock& lowered = program.blocks[block];
  const auto& control = lowered.control;
  if (!lowered.reachable && control.kind != SsaControlTerminatorKind::opaque) {
    throw std::invalid_argument(
        "register-allocation unreachable block has executable control");
  }

  switch (control.kind) {
    case SsaControlTerminatorKind::opaque:
      if (control.predicate.has_value() || control.jump_target.has_value() ||
          control.nonzero_target.has_value() || control.zero_target.has_value()) {
        throw std::invalid_argument(
            "register-allocation opaque control carries payload");
      }
      return std::nullopt;
    case SsaControlTerminatorKind::jump:
      if (control.predicate.has_value() || !control.jump_target.has_value() ||
          control.nonzero_target.has_value() || control.zero_target.has_value()) {
        throw std::invalid_argument(
            "register-allocation jump control has malformed shape");
      }
      validate_control_target(program.graph, block, *control.jump_target);
      return std::nullopt;
    case SsaControlTerminatorKind::branch_if_nonzero:
      if (!control.predicate.has_value() || control.jump_target.has_value() ||
          !control.nonzero_target.has_value() || !control.zero_target.has_value() ||
          control.predicate->kind != SsaCopyLocationKind::value) {
        throw std::invalid_argument(
            "register-allocation conditional control has malformed shape");
      }
      validate_control_target(program.graph, block, *control.nonzero_target);
      validate_control_target(program.graph, block, *control.zero_target);
      return control.predicate;
  }
  throw std::invalid_argument("register-allocation unknown control kind");
}

[[nodiscard]] std::vector<unsigned char> reachable_from(
    const Graph& graph, const Vertex start) {
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

[[nodiscard]] std::vector<Operation> flatten_block(
    const SsaLoweredBlock& block) {
  std::vector<Operation> operations;
  operations.reserve(block.entry_moves.size() + block.instructions.size() +
                     block.exit_moves.size());
  for (std::size_t index = 0U; index < block.entry_moves.size(); ++index) {
    const SsaScheduledMove& move = block.entry_moves[index];
    operations.push_back(Operation{PhiFreeOperationKind::entry_move,
                                   index,
                                   {move.source},
                                   {move.destination}});
  }
  for (std::size_t index = 0U; index < block.instructions.size(); ++index) {
    const SsaInstruction& instruction = block.instructions[index];
    Operation operation;
    operation.kind = PhiFreeOperationKind::instruction;
    operation.index = index;
    operation.uses.reserve(instruction.uses.size());
    for (const SsaValue use : instruction.uses) {
      operation.uses.push_back(SsaCopyLocation::from_value(use));
    }
    if (instruction.definition.has_value()) {
      operation.definitions.push_back(
          SsaCopyLocation::from_value(*instruction.definition));
    }
    operations.push_back(std::move(operation));
  }
  for (std::size_t index = 0U; index < block.exit_moves.size(); ++index) {
    const SsaScheduledMove& move = block.exit_moves[index];
    operations.push_back(Operation{PhiFreeOperationKind::exit_move,
                                   index,
                                   {move.source},
                                   {move.destination}});
  }
  return operations;
}

[[nodiscard]] std::size_t location_index(
    const std::vector<SsaCopyLocation>& locations,
    const SsaCopyLocation location) {
  const auto found = std::lower_bound(locations.begin(), locations.end(), location,
                                      LocationLess{});
  if (found == locations.end() || *found != location) {
    throw std::logic_error(
        "register-allocation location universe is incomplete");
  }
  return static_cast<std::size_t>(found - locations.begin());
}

[[nodiscard]] std::vector<SsaCopyLocation> materialize(
    const Bits& bits, const std::vector<SsaCopyLocation>& locations) {
  std::vector<SsaCopyLocation> result;
  for (std::size_t index = 0U; index < bits.size(); ++index) {
    if (bits[index] != 0U) {
      result.push_back(locations[index]);
    }
  }
  return result;
}

void add_live_clique(const Bits& bits,
                     std::set<std::pair<std::size_t, std::size_t>>& edges) {
  for (std::size_t first = 0U; first < bits.size(); ++first) {
    if (bits[first] == 0U) {
      continue;
    }
    for (std::size_t second = first + 1U; second < bits.size(); ++second) {
      if (bits[second] != 0U) {
        edges.emplace(first, second);
      }
    }
  }
}

}  // namespace

PhiFreeRegisterAllocation allocate_phi_free_registers(
    const OutOfSsaProgram& program, const std::size_t register_budget) {
  if (!program.graph.directed()) {
    throw std::invalid_argument("register allocation requires a directed CFG");
  }
  if (program.blocks.size() != program.graph.vertex_count()) {
    throw std::invalid_argument(
        "register-allocation block count does not match CFG");
  }
  program.graph.validate_vertex(program.start);
  if (program.initial_values.size() != program.variable_count) {
    throw std::invalid_argument(
        "register-allocation initial-value count does not match variables");
  }
  for (Variable variable = 0U; variable < program.variable_count; ++variable) {
    if (program.initial_values[variable] != SsaValue{variable, 0U}) {
      throw std::invalid_argument(
          "register-allocation initial values must be version zero");
    }
  }

  const std::vector<unsigned char> reachable =
      reachable_from(program.graph, program.start);
  std::vector<std::vector<Operation>> operations(program.blocks.size());
  std::vector<std::optional<SsaCopyLocation>> control_predicates(
      program.blocks.size());
  std::vector<SsaCopyLocation> locations;
  locations.reserve(program.initial_values.size() + program.temporary_count);
  for (const SsaValue initial : program.initial_values) {
    insert_location(locations, SsaCopyLocation::from_value(initial),
                    program.variable_count, program.temporary_count);
  }
  for (std::size_t temporary = 0U; temporary < program.temporary_count;
       ++temporary) {
    insert_location(locations, SsaCopyLocation::from_temporary(temporary),
                    program.variable_count, program.temporary_count);
  }

  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    const bool expected_reachable = reachable[block] != 0U;
    if (program.blocks[block].reachable != expected_reachable) {
      throw std::invalid_argument(
          "register-allocation reachable flags do not match CFG");
    }
    operations[block] = flatten_block(program.blocks[block]);
    if (!expected_reachable && !operations[block].empty()) {
      throw std::invalid_argument(
          "unreachable phi-free blocks must not contain operations");
    }
    control_predicates[block] = validate_control(program, block);
    if (control_predicates[block].has_value()) {
      insert_location(locations, *control_predicates[block],
                      program.variable_count, program.temporary_count);
    }
    for (const Operation& operation : operations[block]) {
      for (const SsaCopyLocation use : operation.uses) {
        insert_location(locations, use, program.variable_count,
                        program.temporary_count);
      }
      for (const SsaCopyLocation definition : operation.definitions) {
        insert_location(locations, definition, program.variable_count,
                        program.temporary_count);
      }
    }
  }

  std::sort(locations.begin(), locations.end(), LocationLess{});
  locations.erase(std::unique(locations.begin(), locations.end()),
                  locations.end());
  const std::size_t location_count = locations.size();

  struct IndexedOperation {
    PhiFreeOperationKind kind{PhiFreeOperationKind::instruction};
    std::size_t index{0U};
    std::vector<std::size_t> uses;
    std::vector<std::size_t> definitions;
  };
  std::vector<std::vector<IndexedOperation>> indexed(program.blocks.size());
  std::vector<std::optional<std::size_t>> control_predicate_indices(
      program.blocks.size());
  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    if (control_predicates[block].has_value()) {
      control_predicate_indices[block] =
          location_index(locations, *control_predicates[block]);
    }
    indexed[block].reserve(operations[block].size());
    for (const Operation& operation : operations[block]) {
      IndexedOperation item;
      item.kind = operation.kind;
      item.index = operation.index;
      for (const SsaCopyLocation use : operation.uses) {
        item.uses.push_back(location_index(locations, use));
      }
      for (const SsaCopyLocation definition : operation.definitions) {
        item.definitions.push_back(location_index(locations, definition));
      }
      std::sort(item.uses.begin(), item.uses.end());
      item.uses.erase(std::unique(item.uses.begin(), item.uses.end()),
                      item.uses.end());
      std::sort(item.definitions.begin(), item.definitions.end());
      item.definitions.erase(
          std::unique(item.definitions.begin(), item.definitions.end()),
          item.definitions.end());
      indexed[block].push_back(std::move(item));
    }
  }

  std::vector<Bits> block_use(program.blocks.size(), Bits(location_count, 0U));
  std::vector<Bits> block_def(program.blocks.size(), Bits(location_count, 0U));
  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    if (reachable[block] == 0U) {
      continue;
    }
    for (const IndexedOperation& operation : indexed[block]) {
      for (const std::size_t use : operation.uses) {
        if (block_def[block][use] == 0U) {
          block_use[block][use] = 1U;
        }
      }
      for (const std::size_t definition : operation.definitions) {
        block_def[block][definition] = 1U;
      }
    }
    if (control_predicate_indices[block].has_value()) {
      const std::size_t predicate = *control_predicate_indices[block];
      if (block_def[block][predicate] == 0U) {
        block_use[block][predicate] = 1U;
      }
    }
  }

  std::vector<Bits> live_in(program.blocks.size(), Bits(location_count, 0U));
  std::vector<Bits> live_out(program.blocks.size(), Bits(location_count, 0U));
  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t reverse = program.blocks.size(); reverse > 0U; --reverse) {
      const Vertex block = reverse - 1U;
      if (reachable[block] == 0U) {
        continue;
      }
      Bits next_out(location_count, 0U);
      for (const Edge& edge : program.graph.neighbors(block)) {
        if (reachable[edge.to] == 0U) {
          continue;
        }
        for (std::size_t location = 0U; location < location_count; ++location) {
          if (live_in[edge.to][location] != 0U) {
            next_out[location] = 1U;
          }
        }
      }
      Bits next_in = block_use[block];
      for (std::size_t location = 0U; location < location_count; ++location) {
        if (next_out[location] != 0U && block_def[block][location] == 0U) {
          next_in[location] = 1U;
        }
      }
      if (!same_bits(next_out, live_out[block]) ||
          !same_bits(next_in, live_in[block])) {
        live_out[block] = std::move(next_out);
        live_in[block] = std::move(next_in);
        changed = true;
      }
    }
  }

  PhiFreeRegisterAllocation result;
  result.register_budget = register_budget;
  result.locations = locations;
  result.blocks.resize(program.blocks.size());
  std::set<std::pair<std::size_t, std::size_t>> interference;

  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    PhiFreeBlockLiveness& block_result = result.blocks[block];
    block_result.reachable = reachable[block] != 0U;
    block_result.live_in = materialize(live_in[block], locations);
    block_result.live_out = materialize(live_out[block], locations);
    add_live_clique(live_in[block], interference);
    add_live_clique(live_out[block], interference);

    Bits live = live_out[block];
    if (control_predicate_indices[block].has_value()) {
      live[*control_predicate_indices[block]] = 1U;
      add_live_clique(live, interference);
    }
    std::vector<PhiFreeOperationLiveness> reversed;
    reversed.reserve(indexed[block].size());
    for (std::size_t reverse = indexed[block].size(); reverse > 0U; --reverse) {
      const IndexedOperation& operation = indexed[block][reverse - 1U];
      const Bits after = live;
      for (const std::size_t definition : operation.definitions) {
        live[definition] = 0U;
      }
      for (const std::size_t use : operation.uses) {
        live[use] = 1U;
      }
      const Bits before = live;
      add_live_clique(before, interference);
      add_live_clique(after, interference);
      reversed.push_back(PhiFreeOperationLiveness{
          operation.kind, operation.index, materialize(before, locations),
          materialize(after, locations)});
    }
    std::reverse(reversed.begin(), reversed.end());
    block_result.operations = std::move(reversed);
  }

  result.interference_edges.reserve(interference.size());
  std::vector<std::vector<std::size_t>> adjacency(location_count);
  for (const auto& [first, second] : interference) {
    result.interference_edges.push_back(
        RegisterInterferenceEdge{locations[first], locations[second]});
    adjacency[first].push_back(second);
    adjacency[second].push_back(first);
  }

  std::vector<std::size_t> order(location_count);
  for (std::size_t index = 0U; index < location_count; ++index) {
    order[index] = index;
  }
  std::sort(order.begin(), order.end(),
            [&adjacency, &locations](const std::size_t first,
                                     const std::size_t second) {
              if (adjacency[first].size() != adjacency[second].size()) {
                return adjacency[first].size() > adjacency[second].size();
              }
              return LocationLess{}(locations[first], locations[second]);
            });

  std::vector<std::optional<std::size_t>> assigned(location_count);
  for (const std::size_t location : order) {
    std::set<std::size_t> unavailable;
    for (const std::size_t neighbor : adjacency[location]) {
      if (assigned[neighbor].has_value()) {
        unavailable.insert(*assigned[neighbor]);
      }
    }
    std::size_t candidate = 0U;
    while (candidate < register_budget && unavailable.contains(candidate)) {
      ++candidate;
    }
    if (candidate < register_budget) {
      assigned[location] = candidate;
    }
  }

  result.assignments.reserve(location_count);
  for (std::size_t index = 0U; index < location_count; ++index) {
    result.assignments.push_back(
        PhysicalRegisterAssignment{locations[index], assigned[index]});
    if (!assigned[index].has_value()) {
      result.spills.push_back(locations[index]);
    }
  }
  return result;
}

}  // namespace algorithms::graphs
