#include "algorithms/graphs/backend_register_reservation.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

SsaCopyLocation loc(const std::size_t variable) {
  return SsaCopyLocation::from_value({variable, 0U});
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

void verify_physicalized(const ScratchAwareBackendRegisterPlan& plan) {
  REQUIRE(plan.selection.has_value());
  const auto& selection = *plan.selection;
  REQUIRE_EQ(selection.allocatable_registers +
                 selection.reserved_scratch_registers,
             plan.total_registers);
  REQUIRE_EQ(selection.scratch_physical_registers.size(),
             selection.abstract_lowering.max_scratch_registers);
  REQUIRE_EQ(selection.physicalized_blocks.size(),
             selection.abstract_lowering.blocks.size());

  for (std::size_t scratch = 0U;
       scratch < selection.scratch_physical_registers.size(); ++scratch) {
    REQUIRE_EQ(selection.scratch_physical_registers[scratch],
               selection.allocatable_registers + scratch);
    REQUIRE(selection.scratch_physical_registers[scratch] >=
            selection.allocatable_registers);
    REQUIRE(selection.scratch_physical_registers[scratch] <
            plan.total_registers);
  }

  for (std::size_t block = 0U;
       block < selection.abstract_lowering.blocks.size(); ++block) {
    const auto& before = selection.abstract_lowering.blocks[block];
    const auto& after = selection.physicalized_blocks[block];
    REQUIRE_EQ(before.reachable, after.reachable);
    REQUIRE_EQ(before.original_block, after.original_block);
    REQUIRE_EQ(before.operations.size(), after.operations.size());

    for (std::size_t index = 0U; index < before.operations.size(); ++index) {
      const BackendOperation& original = before.operations[index];
      const BackendOperation& mapped = after.operations[index];
      REQUIRE_EQ(original.kind, mapped.kind);
      REQUIRE_EQ(original.origin_kind, mapped.origin_kind);
      REQUIRE_EQ(original.origin_index, mapped.origin_index);
      REQUIRE_EQ(original.inputs.size(), mapped.inputs.size());

      const auto verify_storage = [&](const BackendStorage source,
                                      const BackendStorage target) {
        if (source.kind == BackendStorageKind::spill_scratch_register) {
          REQUIRE_EQ(target.kind, BackendStorageKind::physical_register);
          REQUIRE(source.index < selection.reserved_scratch_registers);
          REQUIRE_EQ(target.index,
                     selection.allocatable_registers + source.index);
          REQUIRE(target.index >= selection.allocatable_registers);
          REQUIRE(target.index < plan.total_registers);
          return;
        }
        REQUIRE_EQ(target, source);
        if (target.kind == BackendStorageKind::physical_register) {
          REQUIRE(target.index < selection.allocatable_registers);
        }
      };

      for (std::size_t input = 0U; input < original.inputs.size(); ++input) {
        verify_storage(original.inputs[input], mapped.inputs[input]);
      }
      REQUIRE_EQ(original.output.has_value(), mapped.output.has_value());
      if (original.output.has_value()) {
        verify_storage(*original.output, *mapped.output);
      }
    }
  }
}

void verify_attempt_replay(const OutOfSsaProgram& program,
                           const ScratchAwareBackendRegisterPlan& plan) {
  REQUIRE(!plan.attempts.empty());
  for (std::size_t index = 0U; index < plan.attempts.size(); ++index) {
    const auto& attempt = plan.attempts[index];
    REQUIRE_EQ(attempt.reserved_scratch_registers, index);
    REQUIRE_EQ(attempt.allocatable_registers,
               plan.total_registers - attempt.reserved_scratch_registers);
    const auto allocation =
        coalesce_phi_free_registers(program, attempt.allocatable_registers);
    const auto lowering = lower_phi_free_backend_storage(program, allocation);
    REQUIRE_EQ(attempt.measured_scratch_registers,
               lowering.max_scratch_registers);
    REQUIRE_EQ(attempt.stack_slot_count, lowering.stack_slot_count);
    REQUIRE_EQ(attempt.spill_location_count, allocation.spills.size());
    REQUIRE_EQ(attempt.feasible,
               attempt.measured_scratch_registers <=
                   attempt.reserved_scratch_registers);
  }

  if (plan.selection.has_value()) {
    REQUIRE(plan.attempts.back().feasible);
    for (std::size_t index = 0U; index + 1U < plan.attempts.size(); ++index) {
      REQUIRE(!plan.attempts[index].feasible);
    }
    const auto& selected = plan.attempts.back();
    REQUIRE_EQ(plan.selection->reserved_scratch_registers,
               selected.reserved_scratch_registers);
    REQUIRE_EQ(plan.selection->allocatable_registers,
               selected.allocatable_registers);
    REQUIRE_EQ(plan.selection->allocation,
               coalesce_phi_free_registers(program,
                                           selected.allocatable_registers));
    REQUIRE_EQ(plan.selection->abstract_lowering,
               lower_phi_free_backend_storage(program,
                                               plan.selection->allocation));
    verify_physicalized(plan);
  } else {
    REQUIRE_EQ(plan.attempts.size(), plan.total_registers + 1U);
    for (const auto& attempt : plan.attempts) {
      REQUIRE(!attempt.feasible);
    }
  }
}

TEST_CASE(backend_register_reservation_selects_minimal_nonzero_suffix) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});

  const auto plan = plan_scratch_aware_backend_registers(program, 2U);
  REQUIRE(plan.selection.has_value());
  REQUIRE_EQ(plan.selection->reserved_scratch_registers, 1U);
  REQUIRE_EQ(plan.selection->allocatable_registers, 1U);
  REQUIRE_EQ(plan.attempts.size(), 2U);
  REQUIRE(!plan.attempts[0].feasible);
  REQUIRE(plan.attempts[1].feasible);
  REQUIRE_EQ(plan.selection->scratch_physical_registers,
             std::vector<std::size_t>{1U});
  verify_attempt_replay(program, plan);
}

TEST_CASE(backend_register_reservation_accepts_zero_reservation_when_possible) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});

  const auto plan = plan_scratch_aware_backend_registers(program, 2U);
  REQUIRE(plan.selection.has_value());
  REQUIRE_EQ(plan.selection->reserved_scratch_registers, 0U);
  REQUIRE_EQ(plan.selection->allocatable_registers, 2U);
  REQUIRE(plan.selection->scratch_physical_registers.empty());
  REQUIRE_EQ(plan.attempts.size(), 1U);
  verify_attempt_replay(program, plan);
}

TEST_CASE(backend_register_reservation_reports_infeasible_register_file) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});

  const auto plan = plan_scratch_aware_backend_registers(program, 1U);
  REQUIRE(!plan.selection.has_value());
  REQUIRE_EQ(plan.attempts.size(), 2U);
  REQUIRE(!plan.attempts[0].feasible);
  REQUIRE(!plan.attempts[1].feasible);
  verify_attempt_replay(program, plan);
}

TEST_CASE(backend_register_reservation_randomized_replays_phase49_50_pipeline) {
  std::mt19937_64 random(0x51A7C4ULL);
  for (std::size_t trial = 0U; trial < 350U; ++trial) {
    const std::size_t count =
        1U + static_cast<std::size_t>(random() % 7U);
    OutOfSsaProgram program = one_block(count);

    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(random() % 5U); ++index) {
      program.blocks[0].entry_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }

    const std::size_t instruction_count =
        1U + static_cast<std::size_t>(random() % 6U);
    for (std::size_t instruction_index = 0U;
         instruction_index < instruction_count; ++instruction_index) {
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

    const std::size_t total_registers =
        static_cast<std::size_t>(random() % 6U);
    const auto plan =
        plan_scratch_aware_backend_registers(program, total_registers);
    REQUIRE_EQ(plan_scratch_aware_backend_registers(program, total_registers),
               plan);
    verify_attempt_replay(program, plan);
  }
}

}  // namespace
