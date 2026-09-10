#pragma once

#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace algorithms::graphs {

enum class BackendStackPointerMoveDirection : unsigned char {
  toward_lower_addresses,
  toward_higher_addresses,
};

struct BackendStackPointerAdjustmentAction {
  std::size_t stack_pointer_physical_register{0U};
  BackendStackPointerMoveDirection direction{
      BackendStackPointerMoveDirection::toward_lower_addresses};
  std::size_t byte_count{0U};

  friend bool operator==(const BackendStackPointerAdjustmentAction&,
                         const BackendStackPointerAdjustmentAction&) = default;
};

struct BackendFrameBaseMaterializationAction {
  std::size_t frame_base_physical_register{0U};
  std::size_t stack_pointer_physical_register{0U};
  std::int64_t displacement_from_adjusted_stack_pointer{0};

  friend bool operator==(const BackendFrameBaseMaterializationAction&,
                         const BackendFrameBaseMaterializationAction&) = default;
};

using BackendFixedFrameAction =
    std::variant<BackendStackPointerAdjustmentAction,
                 BackendFrameBaseMaterializationAction>;

struct BackendFixedFrameActionPlan {
  StackPointerOwnedBackendFramePlan source_plan;
  std::vector<BackendFixedFrameAction> entry_actions;
  std::vector<BackendFixedFrameAction> exit_actions;

  friend bool operator==(const BackendFixedFrameActionPlan&,
                         const BackendFixedFrameActionPlan&) = default;
};

// Compile the sealed Phase-56 ownership/coordinate witness into an ordered,
// target-neutral fixed-frame action sequence. Successful addressed frames emit
// exactly two entry actions (stack allocation, then frame-base materialization)
// and one exit action (the exact inverse stack movement). Infeasible frames
// emit no actions. Direction + size_t magnitude is used for stack movement so
// the inverse of an INT64_MIN Phase-55 entry displacement remains representable.
[[nodiscard]] BackendFixedFrameActionPlan derive_fixed_frame_actions(
    const StackPointerOwnedBackendFramePlan& source_plan);

}  // namespace algorithms::graphs
