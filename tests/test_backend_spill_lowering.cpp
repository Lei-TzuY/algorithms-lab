#include "algorithms/graphs/backend_spill_lowering.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using namespace algorithms::graphs;

SsaCopyLocation loc(const std::size_t variable) {
  SsaCopyLocation result;
  result.kind = SsaCopyLocationKind::value;
  result.value = {variable, 0U};
  return result;
}

OutOfSsaProgram one_block(const std::size_t variables) {
  OutOfSsaProgram program;
  program.start = 0U;
  program.variable_count = variables;
  program.graph = Graph(1U, true);
  program.blocks.resize(1U);
  program.blocks[0].reachable = true;
  program.blocks[0].original_block = 0U;
  for (std::size_t variable = 0U; variable < variables; ++variable) {
    program.initial_values.push_back({variable, 0U});
  }
  return program;
}

PhiFreeCoalescedRegisterAllocation manual_allocation(
    const std::size_t budget,
    const std::vector<std::optional<std::size_t>>& registers) {
  PhiFreeCoalescedRegisterAllocation result;
  result.register_budget = budget;
  for (std::size_t index = 0U; index < registers.size(); ++index) {
    const SsaCopyLocation location = loc(index);
    result.locations.push_back(location);
    result.classes.push_back({index, {location}, registers[index]});
    result.assignments.push_back({location, registers[index]});
    if (!registers[index].has_value()) result.spills.push_back(location);
  }
  return result;
}

const BackendLocationStorage& binding(
    const PhiFreeBackendSpillLowering& result,
    const SsaCopyLocation location) {
  const auto found = std::find_if(
      result.location_storage.begin(), result.location_storage.end(),
      [location](const BackendLocationStorage& item) {
        return item.location == location;
      });
  if (found == result.location_storage.end()) {
    throw std::runtime_error("missing backend location binding");
  }
  return *found;
}

bool is_register(const BackendStorage storage) {
  return storage.kind == BackendStorageKind::physical_register ||
         storage.kind == BackendStorageKind::spill_scratch_register;
}

void verify_shapes(const PhiFreeBackendSpillLowering& result) {
  std::size_t scratch = 0U;
  for (const BackendLoweredBlock& block : result.blocks) {
    for (const BackendOperation& operation : block.operations) {
      switch (operation.kind) {
        case BackendOperationKind::register_move:
          REQUIRE_EQ(operation.inputs.size(), 1U);
          REQUIRE(operation.output.has_value());
          REQUIRE(is_register(operation.inputs[0]));
          REQUIRE(is_register(*operation.output));
          break;
        case BackendOperationKind::stack_reload:
          REQUIRE_EQ(operation.inputs.size(), 1U);
          REQUIRE(operation.output.has_value());
          REQUIRE_EQ(operation.inputs[0].kind, BackendStorageKind::stack_slot);
          REQUIRE(is_register(*operation.output));
          break;
        case BackendOperationKind::instruction:
          for (const BackendStorage input : operation.inputs) REQUIRE(is_register(input));
          if (operation.output.has_value()) REQUIRE(is_register(*operation.output));
          break;
        case BackendOperationKind::stack_store:
          REQUIRE_EQ(operation.inputs.size(), 1U);
          REQUIRE(operation.output.has_value());
          REQUIRE(is_register(operation.inputs[0]));
          REQUIRE_EQ(operation.output->kind, BackendStorageKind::stack_slot);
          break;
      }
      for (const BackendStorage input : operation.inputs) {
        if (input.kind == BackendStorageKind::spill_scratch_register) {
          scratch = std::max(scratch, input.index + 1U);
        }
      }
      if (operation.output.has_value() &&
          operation.output->kind == BackendStorageKind::spill_scratch_register) {
        scratch = std::max(scratch, operation.output->index + 1U);
      }
    }
  }
  REQUIRE_EQ(result.max_scratch_registers, scratch);
}

TEST_CASE(backend_spill_lowering_materializes_copy_storage_shapes) {
  OutOfSsaProgram program = one_block(5U);
  program.blocks[0].entry_moves = {
      {loc(1U), loc(0U)},
      {loc(2U), loc(1U)},
      {loc(3U), loc(1U)},
      {loc(4U), loc(0U)},
  };
  const auto allocation = manual_allocation(
      2U, {0U, std::nullopt, 1U, std::nullopt, 1U});
  const auto result = lower_phi_free_backend_storage(program, allocation);
  verify_shapes(result);
  REQUIRE_EQ(result.stack_slot_count, 2U);
  REQUIRE_EQ(result.max_scratch_registers, 1U);
  REQUIRE_EQ(result.blocks[0].operations.size(), 5U);
  REQUIRE_EQ(result.blocks[0].operations[0].kind, BackendOperationKind::stack_store);
  REQUIRE_EQ(result.blocks[0].operations[1].kind, BackendOperationKind::stack_reload);
  REQUIRE_EQ(result.blocks[0].operations[2].kind, BackendOperationKind::stack_reload);
  REQUIRE_EQ(result.blocks[0].operations[3].kind, BackendOperationKind::stack_store);
  REQUIRE_EQ(result.blocks[0].operations[4].kind, BackendOperationKind::register_move);
}

TEST_CASE(backend_spill_lowering_omits_equal_final_storage_copy) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].entry_moves.push_back({loc(1U), loc(0U)});
  const auto allocation = manual_allocation(1U, {0U, 0U});
  const auto result = lower_phi_free_backend_storage(program, allocation);
  verify_shapes(result);
  REQUIRE(result.blocks[0].operations.empty());
}

TEST_CASE(backend_spill_lowering_reuses_instruction_scratch) {
  OutOfSsaProgram program = one_block(4U);
  program.blocks[0].instructions.push_back(
      SsaInstruction{{{0U, 0U}, {0U, 0U}, {1U, 0U}, {3U, 0U}},
                     SsaValue{2U, 0U}});
  program.blocks[0].instructions.push_back(
      SsaInstruction{{{0U, 0U}}, SsaValue{0U, 0U}});
  const auto allocation = manual_allocation(
      1U, {std::nullopt, std::nullopt, std::nullopt, 0U});
  const auto result = lower_phi_free_backend_storage(program, allocation);
  verify_shapes(result);
  REQUIRE_EQ(result.max_scratch_registers, 3U);
  const auto& ops = result.blocks[0].operations;
  REQUIRE_EQ(ops.size(), 7U);
  REQUIRE_EQ(ops[0].kind, BackendOperationKind::stack_reload);
  REQUIRE_EQ(ops[1].kind, BackendOperationKind::stack_reload);
  REQUIRE_EQ(ops[2].kind, BackendOperationKind::instruction);
  REQUIRE_EQ(ops[2].inputs.size(), 4U);
  REQUIRE_EQ(ops[2].inputs[0], ops[2].inputs[1]);
  REQUIRE_EQ(ops[3].kind, BackendOperationKind::stack_store);
  REQUIRE_EQ(ops[4].kind, BackendOperationKind::stack_reload);
  REQUIRE_EQ(ops[5].kind, BackendOperationKind::instruction);
  REQUIRE_EQ(ops[5].inputs[0], *ops[5].output);
  REQUIRE_EQ(ops[6].kind, BackendOperationKind::stack_store);
}

TEST_CASE(backend_spill_lowering_rejects_malformed_allocation) {
  OutOfSsaProgram program = one_block(2U);
  auto allocation = manual_allocation(1U, {0U, std::nullopt});
  allocation.assignments[1].physical_register = 0U;
  REQUIRE_THROWS_AS(lower_phi_free_backend_storage(program, allocation),
                    std::invalid_argument);

  allocation = manual_allocation(1U, {0U, std::nullopt});
  allocation.classes[1].members.push_back(loc(0U));
  REQUIRE_THROWS_AS(lower_phi_free_backend_storage(program, allocation),
                    std::invalid_argument);
}

TEST_CASE(backend_spill_lowering_integrates_with_phase49_randomized) {
  std::mt19937_64 random(0x5A11F10FULL);
  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::size_t count = 1U + static_cast<std::size_t>(random() % 7U);
    OutOfSsaProgram program = one_block(count);
    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(random() % 5U); ++index) {
      program.blocks[0].entry_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }
    const std::size_t instruction_count =
        1U + static_cast<std::size_t>(random() % 5U);
    for (std::size_t index = 0U; index < instruction_count; ++index) {
      SsaInstruction instruction;
      for (std::size_t use = 0U;
           use < static_cast<std::size_t>(random() % (count + 2U)); ++use) {
        instruction.uses.push_back(
            {static_cast<std::size_t>(random() % count), 0U});
      }
      if ((random() & 1U) != 0U) {
        instruction.definition =
            SsaValue{static_cast<std::size_t>(random() % count), 0U};
      }
      program.blocks[0].instructions.push_back(std::move(instruction));
    }
    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(random() % 4U); ++index) {
      program.blocks[0].exit_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }

    const std::size_t budget = static_cast<std::size_t>(random() % 4U);
    const auto allocation = coalesce_phi_free_registers(program, budget);
    const auto result = lower_phi_free_backend_storage(program, allocation);
    verify_shapes(result);
    REQUIRE_EQ(lower_phi_free_backend_storage(program, allocation), result);
    REQUIRE_EQ(result.blocks.size(), program.blocks.size());
    REQUIRE_EQ(result.location_storage.size(), allocation.locations.size());

    std::size_t spilled_classes = 0U;
    for (const RegisterCoalescingClass& item : allocation.classes) {
      if (!item.physical_register.has_value()) ++spilled_classes;
    }
    REQUIRE_EQ(result.stack_slot_count, spilled_classes);
    for (const BackendLocationStorage& item : result.location_storage) {
      REQUIRE_EQ(item.storage, binding(result, item.location).storage);
    }
  }
}

}  // namespace
