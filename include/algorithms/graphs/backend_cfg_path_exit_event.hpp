#pragma once

#include "algorithms/graphs/backend_cfg_path_continuation.hpp"
#include "algorithms/graphs/backend_frame_bytecode_execution.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

struct BackendCfgPathExitEventExecution {
  BackendCfgPathContinuationExecution path_execution;
  bool exit_event_consumed{false};
  std::optional<BackendFixedFrameBytecodeExecutionSnapshots>
      fixed_frame_exit_replay;
  std::vector<std::int64_t> final_registers;
  std::vector<std::int64_t> final_frame_slot_values;

  friend bool operator==(const BackendCfgPathExitEventExecution&,
                         const BackendCfgPathExitEventExecution&) = default;
};

// Execute one explicit caller-supplied Phase-64 path and, only if the complete
// finite path finishes without suspension, consume the caller's explicit exit
// event by executing the canonical fixed-frame exit sequence exactly once.
//
// Calling this API is the exit event. Plain Phase-64 path exhaustion through
// execute_backend_cfg_path_continuation() remains non-terminal. Production does
// not infer a return block, branch predicate, successor, or termination
// condition. If any visited block suspends, the exit event remains unconsumed
// and the returned state is exactly the suspended Phase-64 state.
//
// Before accepting the exit, the implementation reconstructs the canonical
// fixed-frame entered state through the sealed Phase-61 executor and requires
// exact equality with the completed Phase-64 terminal registers. Caller-owned
// program/path/register/frame inputs are never mutated.
[[nodiscard]] BackendCfgPathExitEventExecution execute_backend_cfg_path_exit_event(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    std::span<const BackendCfgPathBlockRequest> path,
    std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values);

}  // namespace algorithms::graphs
