#pragma once

#include "algorithms/graphs/backend_frame_actions.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace algorithms::graphs {

struct BackendFixedFrameActionLegalityPolicy {
  std::size_t maximum_stack_adjustment_immediate_magnitude{0U};
  std::int64_t minimum_frame_base_immediate{
      std::numeric_limits<std::int64_t>::min()};
  std::int64_t maximum_frame_base_immediate{
      std::numeric_limits<std::int64_t>::max()};

  friend bool operator==(const BackendFixedFrameActionLegalityPolicy&,
                         const BackendFixedFrameActionLegalityPolicy&) = default;
};

struct LegalizedBackendFixedFrameActionPlan {
  BackendFixedFrameActionPlan source_plan;
  BackendFixedFrameActionLegalityPolicy policy;
  std::vector<BackendFixedFrameAction> entry_actions;
  std::vector<BackendFixedFrameAction> exit_actions;

  friend bool operator==(const LegalizedBackendFixedFrameActionPlan&,
                         const LegalizedBackendFixedFrameActionPlan&) = default;
};

// Legalize the sealed Phase-57 target-neutral fixed-frame action plan under one
// caller-supplied immediate policy. Stack-pointer movements are split into
// deterministic same-direction chunks no larger than the positive magnitude
// limit. The exit chunks are emitted in reverse chunk order with the sealed
// opposite direction, so they replay the exact inverse movement. Frame-base
// materialization remains one action and its signed displacement must fit the
// inclusive configured interval exactly; no narrowing is performed.
//
// Malformed policies and non-canonical/tampered Phase-57 witnesses are rejected.
// Infeasible Phase-57 plans remain successful empty action plans.
[[nodiscard]] LegalizedBackendFixedFrameActionPlan legalize_fixed_frame_actions(
    const BackendFixedFrameActionPlan& source_plan,
    BackendFixedFrameActionLegalityPolicy policy);

}  // namespace algorithms::graphs
