#pragma once

#include "algorithms/graphs/ssa_construction.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

enum class SsaCopyLocationKind : unsigned char {
  value,
  temporary,
};

struct SsaCopyLocation {
  SsaCopyLocationKind kind{SsaCopyLocationKind::value};
  SsaValue value{0U, 0U};
  std::size_t temporary{0U};

  [[nodiscard]] static SsaCopyLocation from_value(SsaValue value);
  [[nodiscard]] static SsaCopyLocation from_temporary(std::size_t temporary);

  friend bool operator==(const SsaCopyLocation&, const SsaCopyLocation&) = default;
};

struct SsaParallelCopy {
  SsaValue destination;
  SsaValue source;
  friend bool operator==(const SsaParallelCopy&, const SsaParallelCopy&) = default;
};

struct SsaScheduledMove {
  SsaCopyLocation destination;
  SsaCopyLocation source;
  friend bool operator==(const SsaScheduledMove&, const SsaScheduledMove&) = default;
};

struct SsaCopySchedule {
  std::vector<SsaScheduledMove> moves;
  std::size_t temporary_count{0U};
  friend bool operator==(const SsaCopySchedule&, const SsaCopySchedule&) = default;
};

// Sequentialize simultaneous copies without clobbering a source that is still
// needed. Duplicate destinations are rejected. Exact self-copies are removed.
// Fresh temporaries are numbered deterministically from zero.
[[nodiscard]] SsaCopySchedule schedule_parallel_copies(
    const std::vector<SsaParallelCopy>& copies);

enum class SsaCopyPlacement : unsigned char {
  predecessor_exit,
  successor_entry,
  split_blocks,
};

struct SsaLoweredBlock {
  bool reachable{false};
  std::optional<Vertex> original_block;
  std::vector<SsaScheduledMove> entry_moves;
  std::vector<SsaInstruction> instructions;
  std::vector<SsaScheduledMove> exit_moves;
  friend bool operator==(const SsaLoweredBlock&, const SsaLoweredBlock&) = default;
};

struct SsaEdgeLowering {
  Vertex predecessor{0U};
  Vertex successor{0U};
  SsaCopyPlacement placement{SsaCopyPlacement::predecessor_exit};
  // One block for predecessor/successor placement. A critical logical edge may
  // have one split block for every physical parallel arc.
  std::vector<Vertex> execution_blocks;
  std::vector<SsaParallelCopy> parallel_copies;
  std::vector<SsaScheduledMove> schedule;
  friend bool operator==(const SsaEdgeLowering&, const SsaEdgeLowering&) = default;
};

struct OutOfSsaProgram {
  Vertex start{0U};
  std::size_t variable_count{0U};
  std::vector<SsaValue> initial_values;
  Graph graph{0U, true};
  std::vector<SsaLoweredBlock> blocks;
  std::vector<SsaEdgeLowering> edge_lowerings;
  std::size_t temporary_count{0U};
};

// Eliminate all phi nodes from a validated scalar SSA program. Original SSA
// instruction value identities are retained as virtual registers; phi semantics
// become deterministic edge-specific copy schedules. Critical logical edges are
// split per physical arc so parallel-edge multiplicity remains explicit.
[[nodiscard]] OutOfSsaProgram destroy_ssa(const Graph& graph,
                                          const SsaProgram& program);

}  // namespace algorithms::graphs
