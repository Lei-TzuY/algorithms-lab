#pragma once

#include "algorithms/graphs/register_allocation.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

enum class CopyCoalescingDecisionKind : unsigned char {
  already_coalesced,
  merged,
  blocked_by_interference,
};

struct CopyPreferenceDecision {
  Vertex block{0U};
  PhiFreeOperationKind kind{PhiFreeOperationKind::entry_move};
  std::size_t index{0U};
  SsaScheduledMove move;
  CopyCoalescingDecisionKind decision{
      CopyCoalescingDecisionKind::already_coalesced};
  friend bool operator==(const CopyPreferenceDecision&,
                         const CopyPreferenceDecision&) = default;
};

struct RegisterCoalescingClass {
  std::size_t class_id{0U};
  std::vector<SsaCopyLocation> members;
  std::optional<std::size_t> physical_register;
  friend bool operator==(const RegisterCoalescingClass&,
                         const RegisterCoalescingClass&) = default;
};

struct QuotientInterferenceEdge {
  std::size_t first_class{0U};
  std::size_t second_class{0U};
  friend bool operator==(const QuotientInterferenceEdge&,
                         const QuotientInterferenceEdge&) = default;
};

struct RedundantScheduledCopy {
  Vertex block{0U};
  PhiFreeOperationKind kind{PhiFreeOperationKind::entry_move};
  std::size_t index{0U};
  SsaScheduledMove move;
  friend bool operator==(const RedundantScheduledCopy&,
                         const RedundantScheduledCopy&) = default;
};

struct PhiFreeCoalescedRegisterAllocation {
  std::size_t register_budget{0U};
  std::vector<SsaCopyLocation> locations;
  std::vector<RegisterInterferenceEdge> original_interference_edges;
  std::vector<CopyPreferenceDecision> preferences;
  std::vector<RegisterCoalescingClass> classes;
  std::vector<QuotientInterferenceEdge> quotient_interference_edges;
  std::vector<PhysicalRegisterAssignment> assignments;
  std::vector<SsaCopyLocation> spills;
  std::vector<RedundantScheduledCopy> redundant_copies;
  friend bool operator==(const PhiFreeCoalescedRegisterAllocation&,
                         const PhiFreeCoalescedRegisterAllocation&) = default;
};

// Reuse the sealed Phase-48 liveness/interference graph, then greedily process
// scheduled move preferences in deterministic program order. Two current
// classes are merged only when no original interference edge crosses between
// them. The resulting quotient graph is recolored under register_budget using
// the same deterministic descending-degree/lowest-register policy as Phase 48.
// Assignments and spills are mapped back to every original location, and every
// scheduled move whose endpoints end in one class is returned as a replayable
// redundant-copy witness. No maximum-coalescing/minimum-register/minimum-spill
// claim is made.
[[nodiscard]] PhiFreeCoalescedRegisterAllocation coalesce_phi_free_registers(
    const OutOfSsaProgram& program, std::size_t register_budget);

}  // namespace algorithms::graphs
