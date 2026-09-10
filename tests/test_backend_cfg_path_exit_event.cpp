#include "algorithms/graphs/backend_cfg_path_exit_event.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <variant>
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
    requests.push_back(
        BackendCfgPathBlockRequest{block, full_replies(plan, block)});
  }
  return requests;
}

void replay_exit_instructions(
    const BackendFixedFrameBytecodePlan& plan,
    std::vector<std::int64_t>& registers) {
  for (const auto& instruction : plan.source_plan.exit_instructions) {
    if (const auto* decrement =
            std::get_if<BackendStackPointerDecrementImmediateInstruction>(
                &instruction)) {
      REQUIRE(decrement->stack_pointer_physical_register < registers.size());
      REQUIRE(decrement->immediate_byte_count <=
              static_cast<std::size_t>(
                  std::numeric_limits<std::int64_t>::max()));
      registers[decrement->stack_pointer_physical_register] -=
          static_cast<std::int64_t>(decrement->immediate_byte_count);
      continue;
    }
    if (const auto* increment =
            std::get_if<BackendStackPointerIncrementImmediateInstruction>(
                &instruction)) {
      REQUIRE(increment->stack_pointer_physical_register < registers.size());
      REQUIRE(increment->immediate_byte_count <=
              static_cast<std::size_t>(
                  std::numeric_limits<std::int64_t>::max()));
      registers[increment->stack_pointer_physical_register] +=
          static_cast<std::int64_t>(increment->immediate_byte_count);
      continue;
    }
    const auto* frame_base =
        std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
            &instruction);
    REQUIRE(frame_base != nullptr);
    REQUIRE(frame_base->frame_base_physical_register < registers.size());
    REQUIRE(frame_base->stack_pointer_physical_register < registers.size());
    registers[frame_base->frame_base_physical_register] =
        registers[frame_base->stack_pointer_physical_register] +
        frame_base->immediate_displacement;
  }
}

TEST_CASE(backend_cfg_path_exit_event_consumes_only_after_completed_path) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  const auto& plan56 = phase56(plan);
  const auto& frame_plan = addressed_frame(plan);

  std::vector<std::int64_t> registers(plan56.total_physical_registers, 700);
  registers[*plan56.stack_pointer_physical_register] = 1000000;
  const std::vector<std::int64_t> frame(frame_plan.stack_slots.size(), 31);
  const auto requests = requests_for(plan, {0U, 1U, 2U, 0U});

  const auto path = execute_backend_cfg_path_continuation(
      plan, program, requests, registers, frame);
  const auto result = execute_backend_cfg_path_exit_event(
      plan, program, requests, registers, frame);

  REQUIRE_EQ(result.path_execution, path);
  REQUIRE(result.exit_event_consumed);
  REQUIRE(result.fixed_frame_exit_replay.has_value());
  REQUIRE_EQ(result.fixed_frame_exit_replay->after_entry_registers,
             path.final_registers);
  REQUIRE_EQ(result.final_registers,
             result.fixed_frame_exit_replay->after_exit_registers);
  REQUIRE_EQ(result.final_frame_slot_values, path.final_frame_slot_values);

  const std::size_t stack_pointer = *plan56.stack_pointer_physical_register;
  REQUIRE(path.final_registers[stack_pointer] != registers[stack_pointer]);
  REQUIRE_EQ(result.final_registers[stack_pointer], registers[stack_pointer]);
}

TEST_CASE(backend_cfg_path_exit_event_suspension_does_not_consume_event) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  const auto& plan56 = phase56(plan);
  std::vector<std::int64_t> registers(plan56.total_physical_registers, 200);
  registers[*plan56.stack_pointer_physical_register] = 1000000;
  const std::vector<std::int64_t> frame(addressed_frame(plan).stack_slots.size(),
                                        41);

  std::vector<BackendCfgPathBlockRequest> requests;
  requests.push_back({0U, full_replies(plan, 0U)});
  requests.push_back({1U, {}});
  requests.push_back({2U, full_replies(plan, 2U)});

  const auto path = execute_backend_cfg_path_continuation(
      plan, program, requests, registers, frame);
  const auto result = execute_backend_cfg_path_exit_event(
      plan, program, requests, registers, frame);

  REQUIRE_EQ(result.path_execution, path);
  REQUIRE(!result.exit_event_consumed);
  REQUIRE(!result.fixed_frame_exit_replay.has_value());
  REQUIRE_EQ(result.final_registers, path.final_registers);
  REQUIRE_EQ(result.final_frame_slot_values, path.final_frame_slot_values);
  REQUIRE_EQ(result.path_execution.completed_path_blocks, std::size_t{1U});
  REQUIRE_EQ(result.path_execution.suspended_at_path_position,
             std::optional<std::size_t>{1U});
  REQUIRE(result.final_registers[*plan56.stack_pointer_physical_register] !=
          registers[*plan56.stack_pointer_physical_register]);
}

TEST_CASE(backend_cfg_path_exit_event_matches_independent_exit_replay) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  const auto& plan56 = phase56(plan);
  const auto& frame_plan = addressed_frame(plan);

  std::mt19937_64 rng(0x65E17E7ULL);
  std::uniform_int_distribution<std::int64_t> value_distribution(-1000000,
                                                                 1000000);
  std::uniform_int_distribution<unsigned int> length_distribution(1U, 18U);

  for (std::size_t trial = 0U; trial < 120U; ++trial) {
    std::vector<std::int64_t> registers(plan56.total_physical_registers, 0);
    for (auto& value : registers) {
      value = value_distribution(rng);
    }
    registers[*plan56.stack_pointer_physical_register] =
        1000000 + static_cast<std::int64_t>(trial);

    std::vector<std::int64_t> frame(frame_plan.stack_slots.size(), 0);
    for (auto& value : frame) {
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

    const auto path = execute_backend_cfg_path_continuation(
        plan, program, requests, registers, frame);
    auto expected_registers = path.final_registers;
    replay_exit_instructions(plan, expected_registers);

    const auto result = execute_backend_cfg_path_exit_event(
        plan, program, requests, registers, frame);
    REQUIRE(result.exit_event_consumed);
    REQUIRE(result.fixed_frame_exit_replay.has_value());
    REQUIRE_EQ(result.path_execution, path);
    REQUIRE_EQ(result.final_registers, expected_registers);
    REQUIRE_EQ(result.final_frame_slot_values, path.final_frame_slot_values);
  }
}

TEST_CASE(backend_cfg_path_exit_event_preserves_caller_inputs_and_validation) {
  const OutOfSsaProgram program = ring_program();
  const BackendFixedFrameBytecodePlan plan = build_plan(program);
  std::vector<std::int64_t> registers(phase56(plan).total_physical_registers,
                                      500);
  registers[*phase56(plan).stack_pointer_physical_register] = 1000000;
  std::vector<std::int64_t> frame(addressed_frame(plan).stack_slots.size(), 55);
  auto requests = requests_for(plan, {0U, 1U, 2U});

  const auto registers_before = registers;
  const auto frame_before = frame;
  const auto requests_before = requests;
  const auto adjacency_before = program.graph.adjacency();
  const auto blocks_before = program.blocks;

  static_cast<void>(execute_backend_cfg_path_exit_event(
      plan, program, requests, registers, frame));
  REQUIRE_EQ(registers, registers_before);
  REQUIRE_EQ(frame, frame_before);
  REQUIRE_EQ(requests, requests_before);
  REQUIRE_EQ(program.graph.adjacency(), adjacency_before);
  REQUIRE_EQ(program.blocks, blocks_before);

  auto tampered = plan;
  tampered.bytes.front() ^= 0xffU;
  REQUIRE_THROWS_AS(execute_backend_cfg_path_exit_event(
                        tampered, program, requests, registers, frame),
                    std::invalid_argument);
}

}  // namespace
