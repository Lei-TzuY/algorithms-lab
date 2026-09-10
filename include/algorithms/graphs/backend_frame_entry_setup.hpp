#pragma once

#include "algorithms/graphs/backend_frame_base_reservation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace algorithms::graphs {

enum class BackendStackGrowthDirection : unsigned char {
  toward_lower_addresses,
  toward_higher_addresses,
};

struct BackendFrameEntrySlotCoordinate {
  std::size_t stack_slot{0U};
  std::size_t byte_offset{0U};
  std::size_t byte_size{0U};
  std::int64_t frame_base_displacement{0};
  std::int64_t entry_stack_pointer_displacement{0};
  friend bool operator==(const BackendFrameEntrySlotCoordinate&,
                         const BackendFrameEntrySlotCoordinate&) = default;
};

struct StackDirectedBackendFrameEntrySetup {
  BackendStackGrowthDirection stack_growth_direction{
      BackendStackGrowthDirection::toward_lower_addresses};
  std::size_t frame_base_physical_register{0U};
  std::size_t frame_size_bytes{0U};
  std::size_t frame_base_byte_anchor{0U};
  std::int64_t stack_pointer_adjustment{0};
  std::int64_t frame_base_from_adjusted_stack_pointer{0};
  std::int64_t frame_base_from_entry_stack_pointer{0};
  std::vector<BackendFrameEntrySlotCoordinate> stack_slots;
  friend bool operator==(const StackDirectedBackendFrameEntrySetup&,
                         const StackDirectedBackendFrameEntrySetup&) = default;
};

struct StackDirectedBackendFramePlan {
  BackendStackGrowthDirection stack_growth_direction{
      BackendStackGrowthDirection::toward_lower_addresses};
  FrameBaseReservedBackendPlan frame_base_plan;
  std::optional<StackDirectedBackendFrameEntrySetup> entry_setup;
  friend bool operator==(const StackDirectedBackendFramePlan&,
                         const StackDirectedBackendFramePlan&) = default;
};

// Derive a target-neutral relation among the function-entry stack pointer, the
// adjusted stack pointer after fixed-size frame allocation, and the dedicated
// Phase-54 frame-base register. No concrete stack-pointer register or machine
// opcode is chosen.
//
// A Phase-54 infeasible plan remains a normal result with entry_setup == nullopt.
// Successful plans preserve the complete Phase-54 plan as replay provenance.
[[nodiscard]] StackDirectedBackendFramePlan
derive_stack_directed_backend_frame_entry(
    const FrameBaseReservedBackendPlan& frame_base_plan,
    BackendStackGrowthDirection stack_growth_direction);

}  // namespace algorithms::graphs
