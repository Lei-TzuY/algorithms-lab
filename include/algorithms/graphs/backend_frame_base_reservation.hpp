#pragma once

#include "algorithms/graphs/backend_frame_addressing.hpp"

#include <cstddef>
#include <optional>

namespace algorithms::graphs {

struct FrameBaseReservedBackendPlan {
  std::size_t total_physical_registers{0U};
  std::optional<std::size_t> frame_base_physical_register;
  ScratchAwareBackendRegisterPlan non_base_register_plan;
  std::optional<ScratchAwareByteAddressedBackendFrame> byte_addressed_frame;
  std::optional<ScratchAwareBaseRelativeBackendFrame> addressed_frame;

  friend bool operator==(const FrameBaseReservedBackendPlan&,
                         const FrameBaseReservedBackendPlan&) = default;
};

// Reserve one dedicated physical register for the logical frame base, then run
// the sealed Phase-51/52/53 pipeline using only the remaining physical-register
// ids. The baseline deterministically reserves the highest id R-1.
//
// R == 0 is a valid infeasible hardware budget: no base register can be
// reserved, so no addressed frame is produced. When R > 0, the Phase-51
// planner receives R-1 registers. A Phase-51 infeasible result is preserved as
// provenance and likewise produces no frame. Frame/layout/addressing
// configuration is evaluated only after a feasible non-base selection exists.
//
// On success every physical-register reference in the addressed frame is
// strictly below frame_base_physical_register, so persistent registers,
// spill-scratch registers, and the dedicated frame-base register are disjoint.
[[nodiscard]] FrameBaseReservedBackendPlan plan_frame_base_reserved_backend(
    const OutOfSsaProgram& program, std::size_t total_physical_registers,
    BackendFrameLayoutConfig frame_layout_config,
    BackendFrameAddressingConfig addressing_config);

}  // namespace algorithms::graphs
