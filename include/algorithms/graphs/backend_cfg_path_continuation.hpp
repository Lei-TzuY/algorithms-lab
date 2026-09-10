#pragma once

#include "algorithms/graphs/backend_instruction_continuation.hpp"
#include "algorithms/graphs/ssa_destruction.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

struct BackendCfgPathBlockRequest {
  Vertex block{0U};
  std::vector<BackendInstructionOracleReply> instruction_replies;

  friend bool operator==(const BackendCfgPathBlockRequest&,
                         const BackendCfgPathBlockRequest&) = default;
};

struct BackendCfgPathContinuationExecution {
  std::vector<BackendInstructionContinuationExecution> block_executions;
  std::vector<std::int64_t> final_registers;
  std::vector<std::int64_t> final_frame_slot_values;
  std::size_t completed_path_blocks{0U};
  std::optional<std::size_t> suspended_at_path_position;

  friend bool operator==(const BackendCfgPathContinuationExecution&,
                         const BackendCfgPathContinuationExecution&) = default;
};

// Execute one explicit caller-supplied path through the sealed out-of-SSA CFG.
// The caller chooses every visited block and supplies one instruction-reply
// script per visit; production never derives a branch predicate or successor.
// The complete backend plan is first rebuilt from `program` plus the policy and
// layout configuration retained by `plan`, and exact equality is required.
//
// The path must start at `program.start`; every consecutive transition must be
// a directed CFG edge. Repeated vertices and self-loop transitions are legal
// when the graph contains the corresponding edge. A suspended Phase-63 block
// stops the path before any successor executes. Caller-owned inputs are never
// mutated.
[[nodiscard]] BackendCfgPathContinuationExecution
execute_backend_cfg_path_continuation(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    std::span<const BackendCfgPathBlockRequest> path,
    std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values);

}  // namespace algorithms::graphs
