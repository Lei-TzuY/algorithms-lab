#pragma once

#include "algorithms/graphs/register_coalescing.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

enum class BackendStorageKind : unsigned char {
  physical_register,
  stack_slot,
  spill_scratch_register,
};

struct BackendStorage {
  BackendStorageKind kind{BackendStorageKind::physical_register};
  std::size_t index{0U};
  friend bool operator==(const BackendStorage&, const BackendStorage&) = default;
};

enum class BackendOperationKind : unsigned char {
  register_move,
  stack_reload,
  instruction,
  stack_store,
};

// Every emitted operation retains the phi-free operation that caused it.
// `inputs` and `output` use BackendStorage uniformly:
// - register_move: register -> register
// - stack_reload: stack slot -> register
// - instruction: register inputs -> optional register output
// - stack_store: register -> stack slot
struct BackendOperation {
  BackendOperationKind kind{BackendOperationKind::instruction};
  PhiFreeOperationKind origin_kind{PhiFreeOperationKind::instruction};
  std::size_t origin_index{0U};
  std::vector<BackendStorage> inputs;
  std::optional<BackendStorage> output;
  friend bool operator==(const BackendOperation&, const BackendOperation&) = default;
};

struct BackendClassStorage {
  std::size_t class_id{0U};
  BackendStorage storage;
  friend bool operator==(const BackendClassStorage&,
                         const BackendClassStorage&) = default;
};

struct BackendLocationStorage {
  SsaCopyLocation location;
  std::size_t class_id{0U};
  BackendStorage storage;
  friend bool operator==(const BackendLocationStorage&,
                         const BackendLocationStorage&) = default;
};

struct BackendLoweredBlock {
  bool reachable{false};
  std::optional<Vertex> original_block;
  std::vector<BackendOperation> operations;
  friend bool operator==(const BackendLoweredBlock&,
                         const BackendLoweredBlock&) = default;
};

struct PhiFreeBackendSpillLowering {
  std::size_t register_budget{0U};
  std::vector<BackendClassStorage> class_storage;
  std::vector<BackendLocationStorage> location_storage;
  std::vector<BackendLoweredBlock> blocks;
  std::size_t stack_slot_count{0U};
  std::size_t max_scratch_registers{0U};
  friend bool operator==(const PhiFreeBackendSpillLowering&,
                         const PhiFreeBackendSpillLowering&) = default;
};

// Materialize a sealed Phase-49 coalesced allocation into a machine-independent
// backend storage/rewrite IR. Assigned coalescing classes retain their physical
// register. Spilled classes receive one deterministic stack slot in class-id
// order. Scheduled copies become register moves / reloads / stores and are
// omitted only when both endpoints already have identical final storage.
// Spilled instruction operands are materialized through operation-local abstract
// scratch registers numbered from zero; the result reports the maximum number
// simultaneously required by any one phi-free operation.
//
// This baseline intentionally does not choose concrete ISA opcodes, byte frame
// offsets/alignment, calling conventions, or hardware scratch registers.
[[nodiscard]] PhiFreeBackendSpillLowering lower_phi_free_backend_storage(
    const OutOfSsaProgram& program,
    const PhiFreeCoalescedRegisterAllocation& allocation);

}  // namespace algorithms::graphs
