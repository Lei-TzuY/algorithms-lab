#pragma once

#include "algorithms/graphs/backend_spill_transfer_execution.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

// One explicit caller-supplied semantic-oracle reply for an opaque body
// instruction. `operation_index` must match the next opaque instruction barrier
// in block order. Instructions with a materialized output require output_value;
// no-output opaque instructions are acknowledged by a reply with no value.
// Known Phase-66 scalar instructions consume no reply.
struct BackendInstructionOracleReply {
  std::size_t operation_index{0U};
  std::optional<std::int64_t> output_value;

  friend bool operator==(const BackendInstructionOracleReply&,
                         const BackendInstructionOracleReply&) = default;
};

// Replay record for one known transfer or body instruction. Transfer steps carry
// transferred_value. Instruction steps carry the observed register inputs plus
// their retained Phase-66 semantic descriptor. A known scalar instruction is
// marked instruction_semantically_evaluated and carries derived_instruction_result;
// an opaque instruction instead distinguishes caller acknowledgement from honest
// suspension through instruction_acknowledged/supplied_instruction_result.
struct BackendInstructionContinuationStep {
  std::size_t operation_index{0U};
  BackendOperationKind kind{BackendOperationKind::register_move};
  PhiFreeOperationKind origin_kind{PhiFreeOperationKind::instruction};
  std::size_t origin_index{0U};
  std::optional<std::int64_t> transferred_value;
  std::vector<std::int64_t> instruction_inputs;
  SsaInstructionSemantics instruction_semantics{};
  bool instruction_semantically_evaluated{false};
  std::optional<std::int64_t> derived_instruction_result;
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

// Continue one canonical Phase-62 block execution. Instructions carrying a
// validated non-opaque Phase-66 scalar descriptor are evaluated from the actual
// materialized register inputs and their checked result is written to the
// already-materialized output register. Opaque instructions retain the sealed
// Phase-63 ordered caller-reply contract and suspend honestly when the reply
// prefix is exhausted. Known scalar instructions never consume oracle replies.
//
// The sealed Phase-62 executor remains the byte/provenance, finite-state, and
// complete-block-preflight trust boundary. This function does not invent CFG
// successor, termination, branch, memory, call, or ABI semantics. Caller-owned
// spans are never mutated.
[[nodiscard]] BackendInstructionContinuationExecution
execute_backend_instruction_continuation_block(
    const BackendFixedFrameBytecodePlan& plan, std::size_t block_index,
    std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values,
    std::span<const BackendInstructionOracleReply> instruction_replies);

}  // namespace algorithms::graphs
