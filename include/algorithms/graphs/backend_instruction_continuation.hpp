#pragma once

#include "algorithms/graphs/backend_spill_transfer_execution.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

// One explicit caller-supplied semantic-oracle reply for an opaque body
// instruction. `operation_index` must match the next instruction barrier in
// block order. Instructions with a materialized output require output_value;
// no-output instructions are acknowledged by a reply with no value.
struct BackendInstructionOracleReply {
  std::size_t operation_index{0U};
  std::optional<std::int64_t> output_value;

  friend bool operator==(const BackendInstructionOracleReply&,
                         const BackendInstructionOracleReply&) = default;
};

// Replay record for either one known transfer or one opaque instruction.
// Transfer steps carry transferred_value. Instruction steps carry the observed
// register inputs and distinguish an acknowledged reply from a suspension.
struct BackendInstructionContinuationStep {
  std::size_t operation_index{0U};
  BackendOperationKind kind{BackendOperationKind::register_move};
  PhiFreeOperationKind origin_kind{PhiFreeOperationKind::instruction};
  std::size_t origin_index{0U};
  std::optional<std::int64_t> transferred_value;
  std::vector<std::int64_t> instruction_inputs;
  bool instruction_acknowledged{false};
  std::optional<std::int64_t> supplied_instruction_result;

  friend bool operator==(const BackendInstructionContinuationStep&,
                         const BackendInstructionContinuationStep&) = default;
};

struct BackendInstructionContinuationExecution {
  std::size_t block_index{0U};
  std::vector<std::int64_t> after_entry_registers;
  std::vector<std::int64_t> after_block_registers;
  std::vector<std::int64_t> frame_slot_values;
  std::vector<BackendInstructionContinuationStep> steps;
  std::size_t consumed_instruction_replies{0U};
  std::optional<std::size_t> suspended_at_instruction;

  friend bool operator==(const BackendInstructionContinuationExecution&,
                         const BackendInstructionContinuationExecution&) = default;
};

// Continue one canonical Phase-62 block execution across an ordered prefix of
// opaque instructions using caller-supplied results/acknowledgements. The
// sealed Phase-62 executor remains the provenance, finite-state, and complete
// block-preflight trust boundary. Production never derives an instruction
// result: it only records current register inputs, applies an explicitly
// supplied result to the already-materialized output register, and resumes the
// known register/frame transfers. If replies are exhausted, execution suspends
// before the next opaque instruction. Caller-owned spans are never mutated.
[[nodiscard]] BackendInstructionContinuationExecution
execute_backend_instruction_continuation_block(
    const BackendFixedFrameBytecodePlan& plan, std::size_t block_index,
    std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values,
    std::span<const BackendInstructionOracleReply> instruction_replies);

}  // namespace algorithms::graphs
