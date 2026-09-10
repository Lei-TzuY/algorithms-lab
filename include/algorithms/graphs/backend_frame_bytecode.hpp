#pragma once

#include "algorithms/graphs/backend_symbolic_frame_instructions.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::graphs {

enum class BackendFixedFrameBytecodeOpcode : std::uint8_t {
  stack_pointer_decrement_immediate = 0x01U,
  stack_pointer_increment_immediate = 0x02U,
  frame_base_from_stack_pointer_immediate = 0x03U,
};

inline constexpr std::uint8_t kBackendFixedFrameBytecodeMagic0 = 0x41U;
inline constexpr std::uint8_t kBackendFixedFrameBytecodeMagic1 = 0x4cU;
inline constexpr std::uint8_t kBackendFixedFrameBytecodeMagic2 = 0x46U;
inline constexpr std::uint8_t kBackendFixedFrameBytecodeMagic3 = 0x42U;
inline constexpr std::uint8_t kBackendFixedFrameBytecodeVersion = 0x01U;

struct BackendFixedFrameBytecodePlan {
  BackendSymbolicFixedFrameInstructionPlan source_plan;
  std::vector<std::uint8_t> bytes;

  friend bool operator==(const BackendFixedFrameBytecodePlan&,
                         const BackendFixedFrameBytecodePlan&) = default;
};

struct DecodedBackendFixedFrameBytecode {
  std::vector<BackendSymbolicFixedFrameInstruction> entry_instructions;
  std::vector<BackendSymbolicFixedFrameInstruction> exit_instructions;

  friend bool operator==(const DecodedBackendFixedFrameBytecode&,
                         const DecodedBackendFixedFrameBytecode&) = default;
};

// Encode one canonical sealed Phase-59 symbolic fixed-frame plan into the
// repository-defined Phase-60 byte format. The complete Phase-59 witness is
// re-derived before serialization; tampered/non-canonical plans are rejected.
//
// Wire integers are fixed-width little-endian uint64 values. Signed int64
// displacements use their mathematical value modulo 2^64 on the wire. This is
// repository bytecode only: it is not an ISA, ABI, object-file, or executable
// machine-code encoding.
[[nodiscard]] BackendFixedFrameBytecodePlan encode_backend_fixed_frame_bytecode(
    const BackendSymbolicFixedFrameInstructionPlan& source_plan);

// Strictly decode one complete Phase-60 byte stream. Unknown magic/version/
// opcode values, truncated fields, impossible instruction counts, values that
// do not fit this host's size_t, and trailing bytes are rejected with
// std::invalid_argument.
[[nodiscard]] DecodedBackendFixedFrameBytecode decode_backend_fixed_frame_bytecode(
    std::span<const std::uint8_t> bytes);

}  // namespace algorithms::graphs
