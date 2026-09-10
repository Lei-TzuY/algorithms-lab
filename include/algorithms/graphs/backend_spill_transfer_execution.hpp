#pragma once

#include "algorithms/graphs/backend_frame_bytecode_execution.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::graphs {

struct BackendSpillTransferExecutionStep {
  std::size_t operation_index{0U};
  BackendOperationKind kind{BackendOperationKind::register_move};
  PhiFreeOperationKind origin_kind{PhiFreeOperationKind::instruction};
  std::size_t origin_index{0U};
  std::int64_t transferred_value{0};

  friend bool operator==(const BackendSpillTransferExecutionStep&,
                         const BackendSpillTransferExecutionStep&) = default;
};

struct BackendSpillTransferBlockExecution {
  std::size_t block_index{0U};
  std::vector<std::int64_t> after_entry_registers;
  std::vector<std::int64_t> after_block_registers;
  // Indexed by the canonical Phase-52/53 stack_slot id, not by byte offset.
  std::vector<std::int64_t> frame_slot_values;
  std::vector<BackendSpillTransferExecutionStep> steps;

  friend bool operator==(const BackendSpillTransferBlockExecution&,
                         const BackendSpillTransferBlockExecution&) = default;
};

// Execute one reachable, transfer-only addressed backend block from the
// provenance retained by a canonical Phase-60 bytecode plan.
//
// The Phase-61 entry sequence is validated/executed first over a finite
// caller-supplied register file. The selected addressed block is then fully
// preflighted before any transfer state is mutated. Supported operations are
// register_move, stack_reload, and stack_store. Opaque `instruction` operations
// are rejected instead of inventing body-instruction semantics.
//
// Frame state is deliberately one signed scalar cell per sealed stack_slot id.
// Base-relative displacements are used only to resolve those canonical cells;
// this is not byte-addressable native memory, an ABI, or ISA execution.
[[nodiscard]] BackendSpillTransferBlockExecution
execute_backend_spill_transfer_block(
    const BackendFixedFrameBytecodePlan& plan, std::size_t block_index,
    std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values);

}  // namespace algorithms::graphs
