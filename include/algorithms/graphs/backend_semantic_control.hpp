#pragma once

#include "algorithms/graphs/backend_instruction_continuation.hpp"
#include "algorithms/graphs/ssa_destruction.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace algorithms::graphs {

enum class BackendSemanticControlStatus : unsigned char {
  body_suspended,
  opaque_control,
  successor_selected,
  returned,
};

struct BackendSemanticControlExecution {
  BackendInstructionContinuationExecution block_execution;
  BackendSemanticControlStatus status{BackendSemanticControlStatus::body_suspended};
  SsaControlTerminatorKind control_kind{SsaControlTerminatorKind::opaque};
  SsaControlTerminationKind termination_kind{SsaControlTerminationKind::none};
  std::optional<std::int64_t> predicate_value;
  std::optional<Vertex> logical_successor;
  std::optional<Vertex> execution_successor;

  friend bool operator==(const BackendSemanticControlExecution&,
                         const BackendSemanticControlExecution&) = default;
};

// Execute one lowered CFG block through the sealed Phase-67 instruction
// continuation boundary, then select a successor only when the canonical backend
// provenance carries explicit Phase-68 control semantics, or report explicit
// Phase-70 void return after the block body completes.
//
// - body_suspended: an opaque body instruction still needs a caller reply;
//   control/termination is not consumed.
// - opaque_control: the body completed, but successor ownership remains with the
//   caller. No CFG edge is guessed from adjacency order, degree, or block id.
// - successor_selected: an explicit jump or branch_if_nonzero selected both its
//   logical source-CFG successor and the concrete lowered execution successor.
// - returned: the body completed and a validated payload-free return_void
//   descriptor terminated a reachable CFG sink; no successor is selected.
//
// Conditional predicates are read from their real persistent backend storage
// after body execution. Physical-register bindings read after_block_registers;
// spilled bindings read the corresponding frame_slot_values entry. Nonzero i64
// selects nonzero_target, zero selects zero_target. This API reports return but
// does not itself execute fixed-frame exit actions; the bounded CFG executor owns
// that exactly-once Phase-70 composition. General memory/call/ABI/target-ISA
// semantics remain outside this boundary.
[[nodiscard]] BackendSemanticControlExecution execute_backend_semantic_control_block(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    Vertex block_index, std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values,
    std::span<const BackendInstructionOracleReply> instruction_replies);

}  // namespace algorithms::graphs
