#pragma once

#include "algorithms/graphs/backend_frame_bytecode_execution.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

struct BackendSpillTransferExecutionStep {
  std::size_t operation_index{0U};
  BackendOperationKind kind{BackendOperationKind::register_move};
  PhiFreeOperationKind origin_kind{PhiFreeOperationKind::instruction};
  std::size_t origin_index{0U};
  // Present only for an executed storage transfer. An opaque instruction
  // barrier deliberately carries no fabricated value.
  std::optional<std::int64_t> transferred_value;

  friend bool operator==(const BackendSpillTransferExecutionStep&,
                         const BackendSpillTransferExecutionStep&) = default;
};

struct BackendSpillTransferBlockExecution {
  std::size_t block_index{0U};
  std::vector<std::int64_t> after_entry_registers;
  // State after all transfer operations that are semantically known. If
  // suspended_at_instruction has a value, this is the state immediately before
  // that opaque instruction.
  std::vector<std::int64_t> after_block_registers;
  // Indexed by the canonical Phase-52/53 stack_slot id, not by byte offset.
  std::vector<std::int64_t> frame_slot_values;
  std::vector<BackendSpillTransferExecutionStep> steps;
  std::optional<std::size_t> suspended_at_instruction;

  friend bool operator==(const BackendSpillTransferBlockExecution&,
                         const BackendSpillTransferBlockExecution&) = default;
};

// Execute the storage-transfer prefix of one reachable addressed backend block
// retained by a canonical Phase-60 bytecode plan.
//
// The Phase-61 entry sequence is validated/executed first over a finite
// caller-supplied register file. Phase 62 additionally replays the retained
// Phase-51 -> Phase-52 -> Phase-53 storage/layout chain and requires the
// addressed body to match that deterministic reconstruction before mutation.
//
// The complete selected block is preflighted before transfer state is mutated.
// register_move, stack_reload, and stack_store are executed exactly. An opaque
// `instruction` validates all register operands, is recorded as a barrier, and
// suspends execution before any unknown computation. Operations after that
// barrier are not executed, so an unknown instruction result can never be
// fabricated and then consumed by a later store.
//
// Frame state is deliberately one signed scalar cell per sealed stack_slot id.
// Base-relative displacements only resolve those canonical cells; this is not
// byte-addressable native memory, an ABI, or ISA execution.
[[nodiscard]] BackendSpillTransferBlockExecution
execute_backend_spill_transfer_block(
    const BackendFixedFrameBytecodePlan& plan, std::size_t block_index,
    std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values);

}  // namespace algorithms::graphs
