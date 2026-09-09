#pragma once

#include "algorithms/graphs/backend_frame_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct BackendFrameAddressingConfig {
  std::size_t frame_base_byte_anchor{0U};
  std::int64_t minimum_displacement{std::numeric_limits<std::int64_t>::min()};
  std::int64_t maximum_displacement{std::numeric_limits<std::int64_t>::max()};
  friend bool operator==(const BackendFrameAddressingConfig&,
                         const BackendFrameAddressingConfig&) = default;
};

struct BaseRelativeBackendFrameSlot {
  std::size_t stack_slot{0U};
  std::size_t byte_offset{0U};
  std::size_t byte_size{0U};
  std::int64_t displacement{0};
  friend bool operator==(const BaseRelativeBackendFrameSlot&,
                         const BaseRelativeBackendFrameSlot&) = default;
};

enum class BaseRelativeBackendStorageKind : unsigned char {
  physical_register,
  frame_base_displacement,
};

struct BaseRelativeBackendStorage {
  BaseRelativeBackendStorageKind kind{
      BaseRelativeBackendStorageKind::physical_register};
  std::size_t physical_register{0U};
  std::int64_t displacement{0};
  friend bool operator==(const BaseRelativeBackendStorage&,
                         const BaseRelativeBackendStorage&) = default;
};

struct BaseRelativeBackendClassStorage {
  std::size_t class_id{0U};
  BaseRelativeBackendStorage storage;
  friend bool operator==(const BaseRelativeBackendClassStorage&,
                         const BaseRelativeBackendClassStorage&) = default;
};

struct BaseRelativeBackendLocationStorage {
  SsaCopyLocation location;
  std::size_t class_id{0U};
  BaseRelativeBackendStorage storage;
  friend bool operator==(const BaseRelativeBackendLocationStorage&,
                         const BaseRelativeBackendLocationStorage&) = default;
};

struct BaseRelativeBackendOperation {
  BackendOperationKind kind{BackendOperationKind::instruction};
  PhiFreeOperationKind origin_kind{PhiFreeOperationKind::instruction};
  std::size_t origin_index{0U};
  std::vector<BaseRelativeBackendStorage> inputs;
  std::optional<BaseRelativeBackendStorage> output;
  friend bool operator==(const BaseRelativeBackendOperation&,
                         const BaseRelativeBackendOperation&) = default;
};

struct BaseRelativeBackendBlock {
  bool reachable{false};
  std::optional<Vertex> original_block;
  std::vector<BaseRelativeBackendOperation> operations;
  friend bool operator==(const BaseRelativeBackendBlock&,
                         const BaseRelativeBackendBlock&) = default;
};

struct ScratchAwareBaseRelativeBackendFrame {
  BackendFrameLayoutConfig frame_layout_config;
  BackendFrameAddressingConfig addressing_config;
  std::size_t total_registers{0U};
  std::size_t reserved_scratch_registers{0U};
  std::size_t allocatable_registers{0U};
  std::vector<std::size_t> scratch_physical_registers;
  std::vector<BaseRelativeBackendFrameSlot> stack_slots;
  std::size_t frame_size_bytes{0U};
  std::vector<BaseRelativeBackendClassStorage> class_storage;
  std::vector<BaseRelativeBackendLocationStorage> location_storage;
  std::vector<BaseRelativeBackendBlock> blocks;
  friend bool operator==(const ScratchAwareBaseRelativeBackendFrame&,
                         const ScratchAwareBaseRelativeBackendFrame&) = default;
};

// Lower a sealed Phase-52 byte-addressed frame into a target-neutral signed
// base-relative displacement view. The logical base anchor is a byte position
// in [0, frame_size_bytes]. Every frame slot start is translated to
// byte_offset - anchor with checked signed arithmetic and must lie inside the
// caller-provided inclusive displacement interval.
//
// Physical-register ids, class/location ids, operation provenance, block
// reachability, and original-block provenance are preserved exactly. This layer
// does not choose a concrete base register, stack direction, ISA addressing
// encoding, calling convention, or prologue/epilogue.
[[nodiscard]] ScratchAwareBaseRelativeBackendFrame
address_scratch_aware_backend_frame(
    const ScratchAwareByteAddressedBackendFrame& frame,
    BackendFrameAddressingConfig config);

}  // namespace algorithms::graphs
