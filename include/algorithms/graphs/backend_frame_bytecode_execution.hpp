#pragma once

#include "algorithms/graphs/backend_frame_bytecode.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::graphs {

struct BackendFixedFrameBytecodeExecutionSnapshots {
  std::vector<std::int64_t> after_entry_registers;
  std::vector<std::int64_t> after_exit_registers;

  friend bool operator==(
      const BackendFixedFrameBytecodeExecutionSnapshots&,
      const BackendFixedFrameBytecodeExecutionSnapshots&) = default;
};

// Execute one canonical Phase-60 fixed-frame bytecode plan over a caller-owned
// finite signed register file. The complete byte stream is strictly decoded and
// the Phase-59 provenance witness is re-encoded before any semantic execution.
// Referenced register ids outside initial_registers are rejected instead of
// growing hidden state. Stack-pointer and frame-base arithmetic is checked.
//
// The input span is read-only. Successful execution returns snapshots after the
// entry and exit sequences; validation or arithmetic failure leaves caller state
// untouched. This is target-neutral educational bytecode semantics, not native
// execution, ABI behavior, memory effects, or target opcode compatibility.
[[nodiscard]] BackendFixedFrameBytecodeExecutionSnapshots
execute_backend_fixed_frame_bytecode(
    const BackendFixedFrameBytecodePlan& plan,
    std::span<const std::int64_t> initial_registers);

}  // namespace algorithms::graphs
