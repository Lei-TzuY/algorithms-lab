#pragma once

#include "algorithms/graphs/backend_frame_entry_setup.hpp"

#include <cstddef>
#include <optional>

namespace algorithms::graphs {

struct StackPointerOwnedBackendFramePlan {
  std::size_t total_physical_registers{0U};
  std::optional<std::size_t> stack_pointer_physical_register;
  StackDirectedBackendFramePlan stack_directed_frame_plan;

  friend bool operator==(const StackPointerOwnedBackendFramePlan&,
                         const StackPointerOwnedBackendFramePlan&) = default;
};

// Reserve the highest physical-register id as a dedicated stack pointer, then
// run the sealed Phase-54 frame-base planner over the remaining register prefix
// and derive the sealed Phase-55 stack-directed frame-entry coordinates.
//
// R == 0 is a valid infeasible hardware budget with no stack-pointer owner. For
// R > 0 the stack pointer is R-1 and the nested Phase-54 planner receives R-1
// registers. Nested register shortage remains a normal result. Whenever a
// frame-base register exists it is below the stack pointer, and every
// persistent/scratch physical register in a successful addressed frame is
// strictly below the frame base.
[[nodiscard]] StackPointerOwnedBackendFramePlan
plan_stack_pointer_owned_backend_frame(
    const OutOfSsaProgram& program, std::size_t total_physical_registers,
    BackendFrameLayoutConfig frame_layout_config,
    BackendFrameAddressingConfig addressing_config,
    BackendStackGrowthDirection stack_growth_direction);

}  // namespace algorithms::graphs
