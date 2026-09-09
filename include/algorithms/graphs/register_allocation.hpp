#pragma once

#include "algorithms/graphs/ssa_destruction.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

enum class PhiFreeOperationKind : unsigned char {
  entry_move,
  instruction,
  exit_move,
};

struct PhiFreeOperationLiveness {
  PhiFreeOperationKind kind{PhiFreeOperationKind::instruction};
  std::size_t index{0U};
  std::vector<SsaCopyLocation> live_before;
  std::vector<SsaCopyLocation> live_after;
  friend bool operator==(const PhiFreeOperationLiveness&,
                         const PhiFreeOperationLiveness&) = default;
};

struct PhiFreeBlockLiveness {
  bool reachable{false};
  std::vector<SsaCopyLocation> live_in;
  std::vector<SsaCopyLocation> live_out;
  std::vector<PhiFreeOperationLiveness> operations;
  friend bool operator==(const PhiFreeBlockLiveness&,
                         const PhiFreeBlockLiveness&) = default;
};

struct RegisterInterferenceEdge {
  SsaCopyLocation first;
  SsaCopyLocation second;
  friend bool operator==(const RegisterInterferenceEdge&,
                         const RegisterInterferenceEdge&) = default;
};

struct PhysicalRegisterAssignment {
  SsaCopyLocation location;
  std::optional<std::size_t> physical_register;
  friend bool operator==(const PhysicalRegisterAssignment&,
                         const PhysicalRegisterAssignment&) = default;
};

struct PhiFreeRegisterAllocation {
  std::size_t register_budget{0U};
  std::vector<SsaCopyLocation> locations;
  std::vector<PhiFreeBlockLiveness> blocks;
  std::vector<RegisterInterferenceEdge> interference_edges;
  std::vector<PhysicalRegisterAssignment> assignments;
  std::vector<SsaCopyLocation> spills;
};

// Compute exact backward may-liveness over the sealed phi-free operation order:
// entry moves, instructions, then exit moves. Interference is reconstructed from
// simultaneous live-before/live-after sets. A deterministic greedy coloring then
// assigns the lowest available physical register in descending-degree order;
// locations that cannot be assigned within register_budget are returned as
// explicit spills. No optimal-coloring/minimum-spill/coalescing claim is made.
[[nodiscard]] PhiFreeRegisterAllocation allocate_phi_free_registers(
    const OutOfSsaProgram& program, std::size_t register_budget);

}  // namespace algorithms::graphs
