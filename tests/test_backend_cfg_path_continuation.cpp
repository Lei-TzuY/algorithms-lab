#include "algorithms/graphs/backend_cfg_path_continuation.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

BackendFixedFrameBytecodePlan encode_phase56(
    const StackPointerOwnedBackendFramePlan& phase56) {
  const BackendFixedFrameActionPlan phase57 = derive_fixed_frame_actions(phase56);
  const BackendFixedFrameActionLegalityPolicy policy{
      8U, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};
  const auto phase58 = legalize_fixed_frame_actions(phase57, policy);
  const auto phase59 = lower_fixed_frame_actions_to_symbolic_instructions(phase58);
  return encode_backend_fixed_frame_bytecode(phase59);
}

OutOfSsaProgram ring_program() {
  OutOfSsaProgram program;
  program.start = 0U;
  program.variable_count = 4U;
  program.graph = Graph(3U, true);
  program.graph.add_edge(0U, 1U);
  program.graph.add_edge(1U, 2U);
  program.graph.add_edge(2U, 0U);
  program.blocks.resize(3U);
  for (std::size_t variable = 0U; variable < program.variable_count; ++variable) {
    program.initial_values.push_back(SsaValue{variable, 0U});
  }
  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    program.blocks[block].reachable = true;
    program.blocks[block].original_block = block;
    for (std::size_t variable = 0U; variable < program.variable_count;
         ++variable) {
      program.blocks[block].instructions.push_back(
          SsaInstruction{{SsaValue{variable, 0U}}, std::nullopt});
    }
  }
  return program;
}

BackendFixedFrameBytecodePlan build_plan(const OutOfSsaProgram& program) {
  const BackendFrameLayoutConfig layout_config{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing_config{0U};
  const auto phase56 = plan_stack_pointer_owned_backend_frame(
      program, 4U, layout_config, addressing_config,
      BackendStackGrowthDirection::toward_lower_addresses);
  return encode_phase56(phase56);
}

const StackPointerOwnedBackendFramePlan& phase56(
    const BackendFixedFrameBytecodePlan& plan) {
  return plan.source_plan.source_plan.source_plan.source_plan;
}

const ScratchAwareBaseRelativeBackendFrame& addressed_frame(
    const BackendFixedFrameBytecodePlan& plan) {
  return *phase56(plan)
              .stack_directed_frame_plan.frame_base_plan.addressed_frame;
}

std::vector<BackendInstructionOracleReply> full_replies(
    const BackendFixedFrameBytecodePlan& plan, const Vertex block) {
  const auto& operations = addressed_frame(plan).blocks[block].operations;
  std::vector<BackendInstructionOracleReply> replies;
  for (std::size_t index = 0U; index < operations.size(); ++index) {
    if (operations[index].kind == BackendOperationKind::instruction) {
      REQUIRE(!operations[index].output.has_value());
      replies.push_back(BackendInstructionOracleReply{index, std::nullopt});
    }
  }
  REQUIRE(!replies.empty());
  return replies;
}

std::vector<BackendCfgPathBlockRequest> requests_for(
    const BackendFixedFrameBytecodePlan& plan,
    const std::vector<Vertex>& blocks) {
  std::vector<BackendCfgPathBlockRequest> requests;
  requests.reserve(blocks.size());
  for (const Vertex block : blocks) {
    requests.push_back(BackendCfgPathBlockRequest{block,
                                                  full_replies(plan, block)});
  }
  return requests;
}

std::map<std::int64_t, std::size_t> slot_map(
    const ScratchAwareBaseRelativeBackendFrame& frame) {
  std::map<std::int64_t, std::size_t> result;
  for (const auto& slot : frame.stack_slots) {
    result.emplace(slot.displacement, slot.stack_slot);
  }
  return result;
}

std::size_t slot_for(const BaseRelativeBackendStorage& storage,
                     const std::map<std::int64_t, std::size_t>& slots) {
  REQUIRE_EQ(storage.kind,
             BaseRelativeBackendStorageKind::frame_base_displacement);
  const auto found = slots.find(storage.displacement);
  REQUIRE(found != slots.end());
  return found->second;
}

void replay_known_block(const BaseRelativeBackendBlock& block,
                        const std::map<std::int64_t, std::size_t>& slots,
                        std::vector<std::int64_t>& registers,
                        std::vector<std::int64_t>& frame_values) {
  for (const auto& operation : block.operations) {
    if (operation.kind == BackendOperationKind::instruction) {
      REQUIRE(!operation.output.has_value());
      continue;
    }
    REQUIRE_EQ(operation.inputs.size(), std::size_t{1U});
    REQUIRE(operation.output.has_value());
    std::int64_t value = 0;
    if (operation.kind == BackendOperationKind::stack_reload) {
      value = frame_values[slot_for(operation.inputs.front(), slots)];
    } else {
      REQUIRE_EQ(operation.inputs.front().kind,
                 BaseRelativeBackendStorageKind::physical_register);
      value = registers[operation.inputs.front().physical_register];
    }

    if (operation.kind == BackendOperationKind::stack_store) {
      frame_values[slot_for(*operation.output, slots)] = value;
    } else {
      REQUIRE_EQ(operation.output->kind,
                 BaseRelativeBackendStorageKind::physical_register);
      registers[operation.output->physical_register] = value;
    }
  }
}

TEST_CASE(backend_cfg_path_continuation_validates_start_edges_and_provenance) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  const std::vector<std::int64_t> registers(
      phase56(plan).total_physical_registers, 1000);
  const std::vector<std::int64_t> frame(addressed_frame(plan).stack_slots.size(),
                                        17);

  const std::vector<BackendCfgPathBlockRequest> empty;
  REQUIRE_THROWS_AS(execute_backend_cfg_path_continuation(
                        plan, program, empty, registers, frame),
                    std::invalid_argument);

  const auto wrong_start = requests_for(plan, {1U});
  REQUIRE_THROWS_AS(execute_backend_cfg_path_continuation(
                        plan, program, wrong_start, registers, frame),
                    std::invalid_argument);

  const auto non_edge = requests_for(plan, {0U, 2U});
  REQUIRE_THROWS_AS(execute_backend_cfg_path_continuation(
                        plan, program, non_edge, registers, frame),
                    std::invalid_argument);

  const std::vector<BackendCfgPathBlockRequest> out_of_range = {
      {0U, full_replies(plan, 0U)}, {3U, {}}};
  REQUIRE_THROWS_AS(execute_backend_cfg_path_continuation(
                        plan, program, out_of_range, registers, frame),
                    std::out_of_range);

  OutOfSsaProgram mismatched = program;
  mismatched.blocks[0].instructions.push_back(SsaInstruction{{}, std::nullopt});
  const auto one_block = requests_for(plan, {0U});
  REQUIRE_THROWS_AS(execute_backend_cfg_path_continuation(
                        plan, mismatched, one_block, registers, frame),
                    std::invalid_argument);

  auto byte_tamper = plan;
  byte_tamper.bytes.front() ^= 0xffU;
  REQUIRE_THROWS_AS(execute_backend_cfg_path_continuation(
                        byte_tamper, program, one_block, registers, frame),
                    std::invalid_argument);
}

TEST_CASE(backend_cfg_path_continuation_hands_state_without_reapplying_entry) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  const auto& plan56 = phase56(plan);
  const auto& frame_plan = addressed_frame(plan);
  REQUIRE(!frame_plan.stack_slots.empty());
  REQUIRE(frame_plan.frame_size_bytes > 0U);

  std::vector<std::int64_t> registers(plan56.total_physical_registers, 0);
  for (std::size_t index = 0U; index < registers.size(); ++index) {
    registers[index] = static_cast<std::int64_t>(100U + index);
  }
  registers[*plan56.stack_pointer_physical_register] = 1000000;
  std::vector<std::int64_t> frame(frame_plan.stack_slots.size(), 0);
  for (std::size_t index = 0U; index < frame.size(); ++index) {
    frame[index] = static_cast<std::int64_t>(900U + index);
  }
  const auto requests = requests_for(plan, {0U, 1U, 2U, 0U});

  const auto result = execute_backend_cfg_path_continuation(
      plan, program, requests, registers, frame);
  REQUIRE(!result.suspended_at_path_position.has_value());
  REQUIRE_EQ(result.completed_path_blocks, requests.size());
  REQUIRE_EQ(result.block_executions.size(), requests.size());
  for (std::size_t index = 1U; index < result.block_executions.size(); ++index) {
    REQUIRE_EQ(result.block_executions[index].after_entry_registers,
               result.block_executions[index - 1U].after_block_registers);
  }

  const auto fixed = execute_backend_fixed_frame_bytecode(plan, registers);
  const std::size_t stack_pointer = *plan56.stack_pointer_physical_register;
  REQUIRE_EQ(result.final_registers[stack_pointer],
             fixed.after_entry_registers[stack_pointer]);
  REQUIRE(result.final_registers[stack_pointer] !=
          fixed.after_exit_registers[stack_pointer]);
  REQUIRE_EQ(registers[*plan56.stack_pointer_physical_register],
             std::int64_t{1000000});
}

TEST_CASE(backend_cfg_path_continuation_suspends_before_following_successor) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  const std::vector<std::int64_t> registers(
      phase56(plan).total_physical_registers, 1000);
  const std::vector<std::int64_t> frame(addressed_frame(plan).stack_slots.size(),
                                        21);

  std::vector<BackendCfgPathBlockRequest> requests;
  requests.push_back({0U, full_replies(plan, 0U)});
  requests.push_back({1U, {}});
  requests.push_back({2U, full_replies(plan, 2U)});

  const auto result = execute_backend_cfg_path_continuation(
      plan, program, requests, registers, frame);
  REQUIRE_EQ(result.completed_path_blocks, std::size_t{1U});
  REQUIRE_EQ(result.suspended_at_path_position,
             std::optional<std::size_t>{1U});
  REQUIRE_EQ(result.block_executions.size(), std::size_t{2U});
  REQUIRE(result.block_executions.back().suspended_at_instruction.has_value());
  REQUIRE_EQ(result.block_executions[1].after_entry_registers,
             result.block_executions[0].after_block_registers);
  REQUIRE_EQ(result.final_registers,
             result.block_executions.back().after_block_registers);
  REQUIRE_EQ(result.final_frame_slot_values,
             result.block_executions.back().frame_slot_values);
}

TEST_CASE(backend_cfg_path_continuation_matches_independent_loop_replay) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  const auto& plan56 = phase56(plan);
  const auto& frame_plan = addressed_frame(plan);
  const auto slots = slot_map(frame_plan);
  REQUIRE(!frame_plan.stack_slots.empty());

  std::mt19937_64 rng(0x64CF6A7AULL);
  std::uniform_int_distribution<std::int64_t> value_distribution(-1000000,
                                                                 1000000);
  std::uniform_int_distribution<unsigned int> length_distribution(1U, 18U);

  for (std::size_t trial = 0U; trial < 120U; ++trial) {
    std::vector<std::int64_t> initial_registers(plan56.total_physical_registers,
                                                0);
    for (auto& value : initial_registers) {
      value = value_distribution(rng);
    }
    initial_registers[*plan56.stack_pointer_physical_register] =
        1000000 + static_cast<std::int64_t>(trial);

    std::vector<std::int64_t> initial_frame(frame_plan.stack_slots.size(), 0);
    for (auto& value : initial_frame) {
      value = value_distribution(rng);
    }

    const std::size_t length =
        static_cast<std::size_t>(length_distribution(rng));
    std::vector<Vertex> blocks;
    blocks.reserve(length);
    for (std::size_t position = 0U; position < length; ++position) {
      blocks.push_back(position % 3U);
    }
    const auto requests = requests_for(plan, blocks);
    const auto actual = execute_backend_cfg_path_continuation(
        plan, program, requests, initial_registers, initial_frame);

    auto expected_registers =
        execute_backend_fixed_frame_bytecode(plan, initial_registers)
            .after_entry_registers;
    auto expected_frame = initial_frame;
    REQUIRE_EQ(actual.block_executions.size(), blocks.size());
    for (std::size_t position = 0U; position < blocks.size(); ++position) {
      REQUIRE_EQ(actual.block_executions[position].after_entry_registers,
                 expected_registers);
      replay_known_block(frame_plan.blocks[blocks[position]], slots,
                         expected_registers, expected_frame);
      REQUIRE_EQ(actual.block_executions[position].after_block_registers,
                 expected_registers);
      REQUIRE_EQ(actual.block_executions[position].frame_slot_values,
                 expected_frame);
    }
    REQUIRE(!actual.suspended_at_path_position.has_value());
    REQUIRE_EQ(actual.completed_path_blocks, blocks.size());
    REQUIRE_EQ(actual.final_registers, expected_registers);
    REQUIRE_EQ(actual.final_frame_slot_values, expected_frame);
  }
}

TEST_CASE(backend_cfg_path_continuation_preserves_caller_inputs) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  std::vector<std::int64_t> registers(phase56(plan).total_physical_registers,
                                      700);
  registers[*phase56(plan).stack_pointer_physical_register] = 1000000;
  std::vector<std::int64_t> frame(addressed_frame(plan).stack_slots.size(), 55);
  auto requests = requests_for(plan, {0U, 1U, 2U});

  const auto registers_before = registers;
  const auto frame_before = frame;
  const auto requests_before = requests;
  const auto adjacency_before = program.graph.adjacency();
  const auto blocks_before = program.blocks;

  static_cast<void>(execute_backend_cfg_path_continuation(
      plan, program, requests, registers, frame));

  REQUIRE_EQ(registers, registers_before);
  REQUIRE_EQ(frame, frame_before);
  REQUIRE_EQ(requests, requests_before);
  REQUIRE_EQ(program.graph.adjacency(), adjacency_before);
  REQUIRE_EQ(program.blocks, blocks_before);
}

}  // namespace
