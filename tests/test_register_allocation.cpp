#include "algorithms/graphs/register_allocation.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

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

struct OracleOperation {
  PhiFreeOperationKind kind;
  std::size_t index;
  std::vector<SsaCopyLocation> uses;
  std::vector<SsaCopyLocation> definitions;
};

std::vector<OracleOperation> oracle_operations(const SsaLoweredBlock& block) {
  std::vector<OracleOperation> result;
  for (std::size_t index = 0U; index < block.entry_moves.size(); ++index) {
    result.push_back({PhiFreeOperationKind::entry_move, index,
                      {block.entry_moves[index].source},
                      {block.entry_moves[index].destination}});
  }
  for (std::size_t index = 0U; index < block.instructions.size(); ++index) {
    OracleOperation operation{PhiFreeOperationKind::instruction, index, {}, {}};
    for (const SsaValue value : block.instructions[index].uses) {
      operation.uses.push_back(SsaCopyLocation::from_value(value));
    }
    if (block.instructions[index].definition.has_value()) {
      operation.definitions.push_back(SsaCopyLocation::from_value(
          *block.instructions[index].definition));
    }
    result.push_back(std::move(operation));
  }
  for (std::size_t index = 0U; index < block.exit_moves.size(); ++index) {
    result.push_back({PhiFreeOperationKind::exit_move, index,
                      {block.exit_moves[index].source},
                      {block.exit_moves[index].destination}});
  }
  return result;
}

bool contains(const std::vector<SsaCopyLocation>& values,
              const SsaCopyLocation value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

bool operation_contains(const std::vector<SsaCopyLocation>& values,
                        const SsaCopyLocation value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

bool oracle_live_from(const OutOfSsaProgram& program,
                      const std::vector<std::vector<OracleOperation>>& operations,
                      const Vertex start_block, const std::size_t start_boundary,
                      const SsaCopyLocation location) {
  struct State {
    Vertex block;
    std::size_t boundary;
  };
  std::vector<std::vector<unsigned char>> seen(program.blocks.size());
  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    seen[block].assign(operations[block].size() + 1U, 0U);
  }
  std::deque<State> pending;
  pending.push_back({start_block, start_boundary});
  while (!pending.empty()) {
    const State state = pending.front();
    pending.pop_front();
    if (seen[state.block][state.boundary] != 0U) {
      continue;
    }
    seen[state.block][state.boundary] = 1U;
    if (state.boundary < operations[state.block].size()) {
      const OracleOperation& operation = operations[state.block][state.boundary];
      if (operation_contains(operation.uses, location)) {
        return true;
      }
      if (!operation_contains(operation.definitions, location)) {
        pending.push_back({state.block, state.boundary + 1U});
      }
      continue;
    }
    for (const Edge& edge : program.graph.neighbors(state.block)) {
      if (program.blocks[edge.to].reachable) {
        pending.push_back({edge.to, 0U});
      }
    }
  }
  return false;
}

std::size_t index_of(const std::vector<SsaCopyLocation>& locations,
                     const SsaCopyLocation location) {
  const auto found = std::find(locations.begin(), locations.end(), location);
  REQUIRE(found != locations.end());
  return static_cast<std::size_t>(found - locations.begin());
}

void insert_canonical_edge(
    std::set<std::pair<std::size_t, std::size_t>>& edges, std::size_t first,
    std::size_t second) {
  if (first == second) {
    return;
  }
  if (second < first) {
    std::swap(first, second);
  }
  edges.emplace(first, second);
}

std::set<std::pair<std::size_t, std::size_t>> expected_interference(
    const OutOfSsaProgram& program,
    const PhiFreeRegisterAllocation& allocation,
    const std::vector<std::vector<OracleOperation>>& operations) {
  std::set<std::pair<std::size_t, std::size_t>> edges;
  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    if (!program.blocks[block].reachable) {
      continue;
    }
    for (std::size_t boundary = 0U; boundary <= operations[block].size();
         ++boundary) {
      std::vector<std::size_t> live;
      for (std::size_t location = 0U; location < allocation.locations.size();
           ++location) {
        if (oracle_live_from(program, operations, block, boundary,
                             allocation.locations[location])) {
          live.push_back(location);
        }
      }
      for (std::size_t first = 0U; first < live.size(); ++first) {
        for (std::size_t second = first + 1U; second < live.size(); ++second) {
          edges.emplace(live[first], live[second]);
        }
      }
    }

    for (std::size_t operation_index = 0U;
         operation_index < operations[block].size(); ++operation_index) {
      const OracleOperation& operation = operations[block][operation_index];
      for (const SsaCopyLocation definition : operation.definitions) {
        const std::size_t definition_index =
            index_of(allocation.locations, definition);
        for (std::size_t location = 0U;
             location < allocation.locations.size(); ++location) {
          if (location == definition_index ||
              !oracle_live_from(program, operations, block,
                                operation_index + 1U,
                                allocation.locations[location])) {
            continue;
          }
          const bool coalescible_copy_source =
              operation.kind != PhiFreeOperationKind::instruction &&
              operation_contains(operation.uses, allocation.locations[location]);
          if (!coalescible_copy_source) {
            insert_canonical_edge(edges, definition_index, location);
          }
        }
      }
    }
  }
  return edges;
}

void verify_against_forward_oracle(const OutOfSsaProgram& program,
                                   const PhiFreeRegisterAllocation& allocation) {
  std::vector<std::vector<OracleOperation>> operations(program.blocks.size());
  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    operations[block] = oracle_operations(program.blocks[block]);
    REQUIRE_EQ(allocation.blocks[block].operations.size(),
               operations[block].size());
  }

  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    if (!program.blocks[block].reachable) {
      REQUIRE(allocation.blocks[block].live_in.empty());
      REQUIRE(allocation.blocks[block].live_out.empty());
      continue;
    }
    for (const SsaCopyLocation location : allocation.locations) {
      REQUIRE_EQ(contains(allocation.blocks[block].live_in, location),
                 oracle_live_from(program, operations, block, 0U, location));
      REQUIRE_EQ(
          contains(allocation.blocks[block].live_out, location),
          oracle_live_from(program, operations, block, operations[block].size(),
                           location));
    }
    for (std::size_t operation = 0U; operation < operations[block].size();
         ++operation) {
      const PhiFreeOperationLiveness& actual =
          allocation.blocks[block].operations[operation];
      REQUIRE_EQ(actual.kind, operations[block][operation].kind);
      REQUIRE_EQ(actual.index, operations[block][operation].index);
      for (const SsaCopyLocation location : allocation.locations) {
        REQUIRE_EQ(contains(actual.live_before, location),
                   oracle_live_from(program, operations, block, operation,
                                    location));
        REQUIRE_EQ(contains(actual.live_after, location),
                   oracle_live_from(program, operations, block, operation + 1U,
                                    location));
      }
    }
  }

  const auto expected = expected_interference(program, allocation, operations);
  std::set<std::pair<std::size_t, std::size_t>> actual;
  for (const RegisterInterferenceEdge& edge : allocation.interference_edges) {
    std::size_t first = index_of(allocation.locations, edge.first);
    std::size_t second = index_of(allocation.locations, edge.second);
    if (second < first) {
      std::swap(first, second);
    }
    actual.emplace(first, second);
  }
  REQUIRE_EQ(actual, expected);

  REQUIRE_EQ(allocation.assignments.size(), allocation.locations.size());
  for (std::size_t index = 0U; index < allocation.locations.size(); ++index) {
    REQUIRE_EQ(allocation.assignments[index].location,
               allocation.locations[index]);
    if (allocation.assignments[index].physical_register.has_value()) {
      REQUIRE(*allocation.assignments[index].physical_register <
              allocation.register_budget);
      REQUIRE(!contains(allocation.spills, allocation.locations[index]));
    } else {
      REQUIRE(contains(allocation.spills, allocation.locations[index]));
    }
  }
  for (const RegisterInterferenceEdge& edge : allocation.interference_edges) {
    const auto first =
        allocation.assignments[index_of(allocation.locations, edge.first)]
            .physical_register;
    const auto second =
        allocation.assignments[index_of(allocation.locations, edge.second)]
            .physical_register;
    if (first.has_value() && second.has_value()) {
      REQUIRE(*first != *second);
    }
  }
}

OutOfSsaProgram one_block_program(const std::size_t variable_count) {
  OutOfSsaProgram program;
  program.start = 0U;
  program.variable_count = variable_count;
  for (Variable variable = 0U; variable < variable_count; ++variable) {
    program.initial_values.push_back({variable, 0U});
  }
  program.graph = Graph(1U, true);
  program.blocks.resize(1U);
  program.blocks[0].reachable = true;
  return program;
}

TEST_CASE(register_allocation_linear_liveness_and_spill) {
  OutOfSsaProgram program = one_block_program(3U);
  const SsaValue a0{0U, 0U};
  const SsaValue b0{1U, 0U};
  const SsaValue c0{2U, 0U};
  program.blocks[0].instructions.push_back({{a0, b0, c0}, std::nullopt});

  const PhiFreeRegisterAllocation allocation =
      allocate_phi_free_registers(program, 2U);
  verify_against_forward_oracle(program, allocation);
  REQUIRE_EQ(allocation.interference_edges.size(), 3U);
  REQUIRE_EQ(allocation.spills.size(), 1U);
  REQUIRE_EQ(allocation.spills[0], SsaCopyLocation::from_value(c0));

  const PhiFreeRegisterAllocation repeated =
      allocate_phi_free_registers(program, 2U);
  REQUIRE_EQ(repeated.assignments, allocation.assignments);
  REQUIRE_EQ(repeated.interference_edges, allocation.interference_edges);
  REQUIRE_EQ(repeated.spills, allocation.spills);
}

TEST_CASE(register_allocation_dead_definition_clobbers_live_through_value) {
  OutOfSsaProgram program = one_block_program(2U);
  const SsaValue a0{0U, 0U};
  const SsaValue b1{1U, 1U};
  program.blocks[0].instructions.push_back({{}, b1});
  program.blocks[0].instructions.push_back({{a0}, std::nullopt});

  const PhiFreeRegisterAllocation allocation =
      allocate_phi_free_registers(program, 1U);
  verify_against_forward_oracle(program, allocation);

  const std::size_t a_index =
      index_of(allocation.locations, SsaCopyLocation::from_value(a0));
  const std::size_t b_index =
      index_of(allocation.locations, SsaCopyLocation::from_value(b1));
  std::size_t first = a_index;
  std::size_t second = b_index;
  if (second < first) {
    std::swap(first, second);
  }
  std::set<std::pair<std::size_t, std::size_t>> actual;
  for (const RegisterInterferenceEdge& edge : allocation.interference_edges) {
    actual.emplace(index_of(allocation.locations, edge.first),
                   index_of(allocation.locations, edge.second));
  }
  REQUIRE(actual.contains({first, second}));
  REQUIRE_EQ(allocation.spills.size(), 1U);
}

TEST_CASE(register_allocation_tracks_moves_temporaries_and_kills) {
  OutOfSsaProgram program = one_block_program(2U);
  program.temporary_count = 1U;
  const SsaValue a0{0U, 0U};
  const SsaValue b0{1U, 0U};
  const SsaValue a1{0U, 1U};
  const SsaCopyLocation temp = SsaCopyLocation::from_temporary(0U);
  program.blocks[0].entry_moves.push_back(
      {temp, SsaCopyLocation::from_value(a0)});
  program.blocks[0].entry_moves.push_back(
      {SsaCopyLocation::from_value(a1), temp});
  program.blocks[0].instructions.push_back({{a1, b0}, std::nullopt});

  const PhiFreeRegisterAllocation allocation =
      allocate_phi_free_registers(program, 2U);
  verify_against_forward_oracle(program, allocation);
  REQUIRE_EQ(allocation.blocks[0].operations.size(), 3U);
  REQUIRE(contains(allocation.blocks[0].operations[0].live_after, temp));
  REQUIRE(!contains(allocation.blocks[0].operations[1].live_after, temp));
}

TEST_CASE(register_allocation_branch_loop_fixpoint_matches_forward_oracle) {
  OutOfSsaProgram program;
  program.start = 0U;
  program.variable_count = 2U;
  program.initial_values = {{0U, 0U}, {1U, 0U}};
  program.graph = Graph(4U, true);
  program.graph.add_edge(0U, 1U);
  program.graph.add_edge(0U, 2U);
  program.graph.add_edge(1U, 3U);
  program.graph.add_edge(2U, 3U);
  program.graph.add_edge(3U, 1U);
  program.blocks.resize(4U);
  for (SsaLoweredBlock& block : program.blocks) {
    block.reachable = true;
  }
  const SsaValue a0{0U, 0U};
  const SsaValue b0{1U, 0U};
  const SsaValue a1{0U, 1U};
  const SsaValue b1{1U, 1U};
  program.blocks[0].instructions.push_back({{a0}, a1});
  program.blocks[1].instructions.push_back({{a1, b0}, b1});
  program.blocks[2].instructions.push_back({{b0}, std::nullopt});
  program.blocks[3].instructions.push_back({{b1}, std::nullopt});

  const PhiFreeRegisterAllocation allocation =
      allocate_phi_free_registers(program, 3U);
  verify_against_forward_oracle(program, allocation);
}

TEST_CASE(register_allocation_validates_phi_free_shape) {
  OutOfSsaProgram undirected = one_block_program(1U);
  undirected.graph = Graph(1U, false);
  REQUIRE_THROWS_AS(allocate_phi_free_registers(undirected, 1U),
                    std::invalid_argument);

  OutOfSsaProgram bad_reachability = one_block_program(1U);
  bad_reachability.blocks[0].reachable = false;
  REQUIRE_THROWS_AS(allocate_phi_free_registers(bad_reachability, 1U),
                    std::invalid_argument);

  OutOfSsaProgram bad_temp = one_block_program(1U);
  bad_temp.blocks[0].entry_moves.push_back(
      {SsaCopyLocation::from_value({0U, 1U}),
       SsaCopyLocation::from_temporary(0U)});
  REQUIRE_THROWS_AS(allocate_phi_free_registers(bad_temp, 1U),
                    std::invalid_argument);
}

TEST_CASE(register_allocation_randomized_forward_path_differential) {
  std::mt19937_64 rng(0xA110CA7EULL);
  for (std::size_t trial = 0U; trial < 260U; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(rng() % 6U);
    const std::size_t variable_count =
        2U + static_cast<std::size_t>(rng() % 4U);
    OutOfSsaProgram program;
    program.start = 0U;
    program.variable_count = variable_count;
    program.graph = Graph(vertex_count, true);
    program.blocks.resize(vertex_count);
    for (Variable variable = 0U; variable < variable_count; ++variable) {
      program.initial_values.push_back({variable, 0U});
    }
    for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
      program.blocks[vertex].reachable = true;
      if (vertex + 1U < vertex_count) {
        program.graph.add_edge(vertex, vertex + 1U);
      }
    }
    for (Vertex from = 0U; from < vertex_count; ++from) {
      for (Vertex to = 0U; to < vertex_count; ++to) {
        if ((rng() % 7U) == 0U) {
          program.graph.add_edge(from, to);
        }
      }
    }

    std::vector<SsaValue> pool = program.initial_values;
    std::vector<std::size_t> next_version(variable_count, 1U);
    program.temporary_count = 2U;
    for (Vertex block = 0U; block < vertex_count; ++block) {
      if ((rng() % 3U) == 0U) {
        const std::size_t temporary =
            static_cast<std::size_t>(rng() % 2U);
        const SsaValue source =
            pool[static_cast<std::size_t>(rng() % pool.size())];
        program.blocks[block].entry_moves.push_back(
            {SsaCopyLocation::from_temporary(temporary),
             SsaCopyLocation::from_value(source)});
      }
      const std::size_t instruction_count =
          static_cast<std::size_t>(rng() % 4U);
      for (std::size_t index = 0U; index < instruction_count; ++index) {
        SsaInstruction instruction;
        const std::size_t use_count =
            static_cast<std::size_t>(rng() % 3U);
        for (std::size_t use = 0U; use < use_count; ++use) {
          instruction.uses.push_back(
              pool[static_cast<std::size_t>(rng() % pool.size())]);
        }
        if ((rng() & 1U) != 0U) {
          const Variable variable =
              static_cast<Variable>(rng() % variable_count);
          const SsaValue definition{variable, next_version[variable]++};
          instruction.definition = definition;
          pool.push_back(definition);
        }
        program.blocks[block].instructions.push_back(std::move(instruction));
      }
      if ((rng() % 4U) == 0U) {
        const std::size_t temporary =
            static_cast<std::size_t>(rng() % 2U);
        const SsaValue destination{
            static_cast<Variable>(rng() % variable_count),
            next_version[static_cast<std::size_t>(rng() % variable_count)]};
        // Keep the destination variable/version deterministic and valid without
        // introducing it into the instruction pool; moves are phi-result defs.
        const Variable variable =
            static_cast<Variable>(rng() % variable_count);
        const SsaValue move_destination{variable, next_version[variable]++};
        static_cast<void>(destination);
        program.blocks[block].exit_moves.push_back(
            {SsaCopyLocation::from_value(move_destination),
             SsaCopyLocation::from_temporary(temporary)});
      }
    }

    const std::size_t budget = static_cast<std::size_t>(rng() % 5U);
    const PhiFreeRegisterAllocation allocation =
        allocate_phi_free_registers(program, budget);
    verify_against_forward_oracle(program, allocation);
  }
}

}  // namespace
