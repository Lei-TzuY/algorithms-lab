#pragma once

#include "algorithms/graphs/backend_register_reservation.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct BackendFrameLayoutConfig {
  std::size_t stack_slot_size_bytes{8U};
  std::size_t stack_slot_alignment_bytes{8U};
  std::size_t frame_alignment_bytes{16U};
  friend bool operator==(const BackendFrameLayoutConfig&,
                         const BackendFrameLayoutConfig&) = default;
};

struct BackendFrameSlotLayout {
  std::size_t stack_slot{0U};
  std::size_t byte_offset{0U};
  std::size_t byte_size{0U};
  friend bool operator==(const BackendFrameSlotLayout&,
                         const BackendFrameSlotLayout&) = default;
};

enum class ByteAddressedBackendStorageKind : unsigned char {
  physical_register,
  frame_byte_offset,
};

struct ByteAddressedBackendStorage {
  ByteAddressedBackendStorageKind kind{
      ByteAddressedBackendStorageKind::physical_register};
  std::size_t index{0U};
  friend bool operator==(const ByteAddressedBackendStorage&,
                         const ByteAddressedBackendStorage&) = default;
};

struct ByteAddressedBackendClassStorage {
  std::size_t class_id{0U};
  ByteAddressedBackendStorage storage;
  friend bool operator==(const ByteAddressedBackendClassStorage&,
                         const ByteAddressedBackendClassStorage&) = default;
};

struct ByteAddressedBackendLocationStorage {
  SsaCopyLocation location;
  std::size_t class_id{0U};
  ByteAddressedBackendStorage storage;
  friend bool operator==(const ByteAddressedBackendLocationStorage&,
                         const ByteAddressedBackendLocationStorage&) = default;
};

struct ByteAddressedBackendOperation {
  BackendOperationKind kind{BackendOperationKind::instruction};
  PhiFreeOperationKind origin_kind{PhiFreeOperationKind::instruction};
  std::size_t origin_index{0U};
  std::vector<ByteAddressedBackendStorage> inputs;
  std::optional<ByteAddressedBackendStorage> output;
  friend bool operator==(const ByteAddressedBackendOperation&,
                         const ByteAddressedBackendOperation&) = default;
};

struct ByteAddressedBackendBlock {
  bool reachable{false};
  std::optional<Vertex> original_block;
  std::vector<ByteAddressedBackendOperation> operations;
  friend bool operator==(const ByteAddressedBackendBlock&,
                         const ByteAddressedBackendBlock&) = default;
};

struct ScratchAwareByteAddressedBackendFrame {
  BackendFrameLayoutConfig config;
  std::size_t total_registers{0U};
  std::size_t reserved_scratch_registers{0U};
  std::size_t allocatable_registers{0U};
  std::vector<std::size_t> scratch_physical_registers;
  std::vector<BackendFrameSlotLayout> stack_slots;
  std::size_t frame_size_bytes{0U};
  std::vector<ByteAddressedBackendClassStorage> class_storage;
  std::vector<ByteAddressedBackendLocationStorage> location_storage;
  std::vector<ByteAddressedBackendBlock> blocks;
  friend bool operator==(const ScratchAwareByteAddressedBackendFrame&,
                         const ScratchAwareByteAddressedBackendFrame&) = default;
};

// Convert the sealed Phase-51 selection into a target-neutral byte-addressed
// spill-frame view. Every abstract stack slot receives one deterministic byte
// offset. Physical-register ids and all operation/block provenance are preserved.
//
// The caller supplies one uniform spill-slot size/alignment plus an overall frame
// alignment. Alignments must be nonzero powers of two and frame alignment must be
// at least the slot alignment. All offset/size arithmetic is checked.
//
// This layer deliberately does not choose a stack-growth direction, concrete ISA
// addressing mode, calling convention, frame pointer, or prologue/epilogue.
[[nodiscard]] ScratchAwareByteAddressedBackendFrame
layout_scratch_aware_backend_frame(
    const ScratchAwareBackendRegisterSelection& selection,
    BackendFrameLayoutConfig config);

}  // namespace algorithms::graphs
