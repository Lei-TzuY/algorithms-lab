#pragma once

#include "algorithms/graphs/backend_frame_action_legalization.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace algorithms::graphs {

struct BackendStackPointerDecrementImmediateInstruction {
  std::size_t stack_pointer_physical_register{0U};
  std::size_t immediate_byte_count{0U};

  friend bool operator==(
      const BackendStackPointerDecrementImmediateInstruction&,
      const BackendStackPointerDecrementImmediateInstruction&) = default;
};

struct BackendStackPointerIncrementImmediateInstruction {
  std::size_t stack_pointer_physical_register{0U};
  std::size_t immediate_byte_count{0U};

  friend bool operator==(
      const BackendStackPointerIncrementImmediateInstruction&,
      const BackendStackPointerIncrementImmediateInstruction&) = default;
};

struct BackendFrameBaseFromStackPointerImmediateInstruction {
  std::size_t frame_base_physical_register{0U};
  std::size_t stack_pointer_physical_register{0U};
  std::int64_t immediate_displacement{0};

  friend bool operator==(
      const BackendFrameBaseFromStackPointerImmediateInstruction&,
      const BackendFrameBaseFromStackPointerImmediateInstruction&) = default;
};

using BackendSymbolicFixedFrameInstruction =
    std::variant<BackendStackPointerDecrementImmediateInstruction,
                 BackendStackPointerIncrementImmediateInstruction,
                 BackendFrameBaseFromStackPointerImmediateInstruction>;

struct BackendSymbolicFixedFrameInstructionPlan {
  LegalizedBackendFixedFrameActionPlan source_plan;
  std::vector<BackendSymbolicFixedFrameInstruction> entry_instructions;
  std::vector<BackendSymbolicFixedFrameInstruction> exit_instructions;

  friend bool operator==(const BackendSymbolicFixedFrameInstructionPlan&,
                         const BackendSymbolicFixedFrameInstructionPlan&) = default;
};

// Lower the sealed Phase-58 legalized fixed-frame action sequence into explicit
// target-neutral symbolic instruction forms. The complete Phase-58 witness is
// re-derived before selection; tampered/non-canonical witnesses are rejected.
// Physical-register ids and already-legal immediates are preserved exactly.
// Infeasible upstream plans remain successful empty instruction sequences.
//
// This is symbolic instruction selection only: no concrete opcode bytes,
// machine register names, ABI behavior, or encoding-size optimality is implied.
[[nodiscard]] BackendSymbolicFixedFrameInstructionPlan
lower_fixed_frame_actions_to_symbolic_instructions(
    const LegalizedBackendFixedFrameActionPlan& source_plan);

}  // namespace algorithms::graphs
