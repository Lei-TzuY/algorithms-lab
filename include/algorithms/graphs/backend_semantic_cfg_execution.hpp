#pragma once

#include "algorithms/graphs/backend_semantic_control.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

// One caller-supplied opaque-instruction reply script for one dynamic CFG visit.
// The request deliberately carries no block id or successor; production derives
// the visited block only from program.start and sealed Phase-68 semantic control.
struct BackendSemanticCfgVisitRequest {
  std::vector<BackendInstructionOracleReply> instruction_replies;

  friend bool operator==(const BackendSemanticCfgVisitRequest&,
                         const BackendSemanticCfgVisitRequest&) = default;
};

enum class BackendSemanticCfgStopReason : unsigned char {
  body_suspended,
  opaque_control,
  visit_budget_exhausted,
};

struct BackendSemanticCfgExecution {
  std::vector<BackendSemanticControlExecution> visits;
  std::vector<std::int64_t> final_registers;
  std::vector<std::int64_t> final_frame_slot_values;
  std::size_t completed_visits{0U};
  BackendSemanticCfgStopReason stop_reason{
      BackendSemanticCfgStopReason::visit_budget_exhausted};
  std::optional<std::size_t> suspended_at_visit;
  std::optional<Vertex> next_execution_block;

  friend bool operator==(const BackendSemanticCfgExecution&,
                         const BackendSemanticCfgExecution&) = default;
};

// Execute a bounded dynamic walk through one retained out-of-SSA program.
//
// Execution always begins at program.start. Every completed visit reuses the
// sealed Phase-68 block executor and follows only its explicit concrete
// execution_successor; callers supply opaque-instruction replies per dynamic
// visit but cannot choose blocks or CFG edges. Register/frame state is carried
// between visits using the sealed Phase-64 handoff invariant.
//
// visit_requests.size() must equal visit_budget and visit_budget must be nonzero.
// The executor may stop before consuming all requests when a body suspends or
// when control remains opaque. If a successor is selected on the final allowed
// visit, stop_reason is visit_budget_exhausted and next_execution_block records
// the exact concrete block that would execute next.
//
// Budget exhaustion is only a finite-resource boundary. This API does not infer
// return/function termination, execute fixed-frame exit actions, interpret opaque
// control, or add general memory/call/ABI/target-ISA semantics.
[[nodiscard]] BackendSemanticCfgExecution execute_backend_semantic_cfg(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    std::size_t visit_budget,
    std::span<const BackendSemanticCfgVisitRequest> visit_requests,
    std::span<const std::int64_t> initial_registers,
    std::span<const std::int64_t> initial_frame_slot_values);

}  // namespace algorithms::graphs
