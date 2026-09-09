#pragma once

#include "algorithms/graphs/backend_spill_lowering.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct BackendRegisterReservationAttempt {
  std::size_t reserved_scratch_registers{0U};
  std::size_t allocatable_registers{0U};
  std::size_t measured_scratch_registers{0U};
  std::size_t stack_slot_count{0U};
  std::size_t spill_location_count{0U};
  bool feasible{false};
  friend bool operator==(const BackendRegisterReservationAttempt&,
                         const BackendRegisterReservationAttempt&) = default;
};

struct ScratchAwareBackendRegisterSelection {
  std::size_t reserved_scratch_registers{0U};
  std::size_t allocatable_registers{0U};
  PhiFreeCoalescedRegisterAllocation allocation;
  PhiFreeBackendSpillLowering abstract_lowering;
  std::vector<std::size_t> scratch_physical_registers;
  std::vector<BackendLoweredBlock> physicalized_blocks;
  friend bool operator==(const ScratchAwareBackendRegisterSelection&,
                         const ScratchAwareBackendRegisterSelection&) = default;
};

struct ScratchAwareBackendRegisterPlan {
  std::size_t total_registers{0U};
  std::vector<BackendRegisterReservationAttempt> attempts;
  std::optional<ScratchAwareBackendRegisterSelection> selection;
  friend bool operator==(const ScratchAwareBackendRegisterPlan&,
                         const ScratchAwareBackendRegisterPlan&) = default;
};

[[nodiscard]] ScratchAwareBackendRegisterPlan
plan_scratch_aware_backend_registers(const OutOfSsaProgram& program,
                                     std::size_t total_registers);

}  // namespace algorithms::graphs
