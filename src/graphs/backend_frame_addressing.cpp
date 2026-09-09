#include "algorithms/graphs/backend_frame_addressing.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

[[nodiscard]] bool is_power_of_two(const std::size_t value) noexcept {
  return value != 0U && (value & (value - 1U)) == 0U;
}

[[nodiscard]] std::size_t checked_add(const std::size_t left,
                                      const std::size_t right) {
  if (left > std::numeric_limits<std::size_t>::max() - right) {
    throw std::overflow_error("base-relative frame size arithmetic overflow");
  }
  return left + right;
}

[[nodiscard]] std::size_t checked_align_up(const std::size_t value,
                                           const std::size_t alignment) {
  const std::size_t remainder = value % alignment;
  if (remainder == 0U) {
    return value;
  }
  return checked_add(value, alignment - remainder);
}

void validate_layout_config(const BackendFrameLayoutConfig config) {
  if (config.stack_slot_size_bytes == 0U) {
    throw std::logic_error("base-relative source frame has zero slot size");
  }
  if (!is_power_of_two(config.stack_slot_alignment_bytes) ||
      !is_power_of_two(config.frame_alignment_bytes) ||
      config.frame_alignment_bytes < config.stack_slot_alignment_bytes) {
    throw std::logic_error("base-relative source frame has invalid alignment");
  }
}

[[nodiscard]] std::vector<std::size_t> validate_slot_layout(
    const ScratchAwareByteAddressedBackendFrame& frame) {
  validate_layout_config(frame.config);
  std::vector<std::size_t> offsets;
  offsets.reserve(frame.stack_slots.size());
  std::size_t cursor = 0U;
  for (std::size_t slot = 0U; slot < frame.stack_slots.size(); ++slot) {
    const BackendFrameSlotLayout& item = frame.stack_slots[slot];
    const std::size_t expected_offset =
        checked_align_up(cursor, frame.config.stack_slot_alignment_bytes);
    if (item.stack_slot != slot || item.byte_offset != expected_offset ||
        item.byte_size != frame.config.stack_slot_size_bytes) {
      throw std::logic_error(
          "base-relative source frame slot layout is inconsistent");
    }
    cursor = checked_add(expected_offset, item.byte_size);
    offsets.push_back(item.byte_offset);
  }
  const std::size_t expected_frame_size =
      checked_align_up(cursor, frame.config.frame_alignment_bytes);
  if (frame.frame_size_bytes != expected_frame_size) {
    throw std::logic_error(
        "base-relative source frame size is inconsistent with slot layout");
  }
  return offsets;
}

void validate_register_witness(
    const ScratchAwareByteAddressedBackendFrame& frame) {
  const std::size_t expected_total = checked_add(
      frame.allocatable_registers, frame.reserved_scratch_registers);
  if (frame.total_registers != expected_total ||
      frame.scratch_physical_registers.size() >
          frame.reserved_scratch_registers) {
    throw std::logic_error(
        "base-relative source frame register counts are inconsistent");
  }
  for (std::size_t scratch = 0U;
       scratch < frame.scratch_physical_registers.size(); ++scratch) {
    const std::size_t expected =
        checked_add(frame.allocatable_registers, scratch);
    if (expected >= frame.total_registers ||
        frame.scratch_physical_registers[scratch] != expected) {
      throw std::logic_error(
          "base-relative source frame scratch-register witness is inconsistent");
    }
  }
}

[[nodiscard]] bool has_slot_offset(const std::vector<std::size_t>& offsets,
                                   const std::size_t offset) {
  return std::binary_search(offsets.begin(), offsets.end(), offset);
}

void validate_source_storage(const ByteAddressedBackendStorage storage,
                             const std::vector<std::size_t>& offsets,
                             const std::size_t register_limit) {
  switch (storage.kind) {
    case ByteAddressedBackendStorageKind::physical_register:
      if (storage.index >= register_limit) {
        throw std::logic_error(
            "base-relative source frame register reference is out of range");
      }
      return;
    case ByteAddressedBackendStorageKind::frame_byte_offset:
      if (!has_slot_offset(offsets, storage.index)) {
        throw std::logic_error(
            "base-relative source frame byte offset is not a slot start");
      }
      return;
  }
  throw std::logic_error("base-relative source frame has unknown storage kind");
}

void validate_source_frame(const ScratchAwareByteAddressedBackendFrame& frame,
                           const std::vector<std::size_t>& offsets) {
  validate_register_witness(frame);
  for (const ByteAddressedBackendClassStorage& item : frame.class_storage) {
    validate_source_storage(item.storage, offsets, frame.allocatable_registers);
  }
  for (const ByteAddressedBackendLocationStorage& item :
       frame.location_storage) {
    validate_source_storage(item.storage, offsets, frame.allocatable_registers);
  }
  for (const ByteAddressedBackendBlock& block : frame.blocks) {
    for (const ByteAddressedBackendOperation& operation : block.operations) {
      for (const ByteAddressedBackendStorage input : operation.inputs) {
        validate_source_storage(input, offsets, frame.total_registers);
      }
      if (operation.output.has_value()) {
        validate_source_storage(*operation.output, offsets,
                                frame.total_registers);
      }
    }
  }
}

[[nodiscard]] std::int64_t checked_displacement(
    const std::size_t byte_offset, const std::size_t anchor) {
  if (byte_offset >= anchor) {
    const std::uintmax_t magnitude =
        static_cast<std::uintmax_t>(byte_offset - anchor);
    const auto positive_limit = static_cast<std::uintmax_t>(
        std::numeric_limits<std::int64_t>::max());
    if (magnitude > positive_limit) {
      throw std::overflow_error(
          "base-relative positive displacement is not int64 representable");
    }
    return static_cast<std::int64_t>(magnitude);
  }

  const std::uintmax_t magnitude =
      static_cast<std::uintmax_t>(anchor - byte_offset);
  const auto negative_limit =
      static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max()) +
      std::uintmax_t{1};
  if (magnitude > negative_limit) {
    throw std::overflow_error(
        "base-relative negative displacement is not int64 representable");
  }
  if (magnitude == negative_limit) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return -static_cast<std::int64_t>(magnitude);
}

void validate_addressing_config(
    const ScratchAwareByteAddressedBackendFrame& frame,
    const BackendFrameAddressingConfig config) {
  if (config.minimum_displacement > config.maximum_displacement) {
    throw std::invalid_argument(
        "base-relative displacement interval is inverted");
  }
  if (config.frame_base_byte_anchor > frame.frame_size_bytes) {
    throw std::out_of_range("base-relative frame anchor is outside frame");
  }
}

void validate_policy(const std::int64_t displacement,
                     const BackendFrameAddressingConfig config) {
  if (displacement < config.minimum_displacement ||
      displacement > config.maximum_displacement) {
    throw std::out_of_range(
        "base-relative displacement is outside caller policy");
  }
}

[[nodiscard]] std::int64_t find_displacement(
    const std::vector<BaseRelativeBackendFrameSlot>& slots,
    const std::size_t byte_offset) {
  const auto iterator = std::lower_bound(
      slots.begin(), slots.end(), byte_offset,
      [](const BaseRelativeBackendFrameSlot& slot, const std::size_t value) {
        return slot.byte_offset < value;
      });
  if (iterator == slots.end() || iterator->byte_offset != byte_offset) {
    throw std::logic_error(
        "base-relative source offset lost validated slot mapping");
  }
  return iterator->displacement;
}

[[nodiscard]] BaseRelativeBackendStorage map_storage(
    const ByteAddressedBackendStorage storage,
    const std::vector<BaseRelativeBackendFrameSlot>& slots) {
  if (storage.kind == ByteAddressedBackendStorageKind::physical_register) {
    return {BaseRelativeBackendStorageKind::physical_register, storage.index,
            0};
  }
  if (storage.kind == ByteAddressedBackendStorageKind::frame_byte_offset) {
    return {BaseRelativeBackendStorageKind::frame_base_displacement, 0U,
            find_displacement(slots, storage.index)};
  }
  throw std::logic_error("base-relative mapping saw unknown storage kind");
}

}  // namespace

ScratchAwareBaseRelativeBackendFrame address_scratch_aware_backend_frame(
    const ScratchAwareByteAddressedBackendFrame& frame,
    const BackendFrameAddressingConfig config) {
  validate_addressing_config(frame, config);
  const std::vector<std::size_t> offsets = validate_slot_layout(frame);
  validate_source_frame(frame, offsets);

  ScratchAwareBaseRelativeBackendFrame result;
  result.frame_layout_config = frame.config;
  result.addressing_config = config;
  result.total_registers = frame.total_registers;
  result.reserved_scratch_registers = frame.reserved_scratch_registers;
  result.allocatable_registers = frame.allocatable_registers;
  result.scratch_physical_registers = frame.scratch_physical_registers;
  result.frame_size_bytes = frame.frame_size_bytes;
  result.stack_slots.reserve(frame.stack_slots.size());
  for (const BackendFrameSlotLayout& slot : frame.stack_slots) {
    const std::int64_t displacement =
        checked_displacement(slot.byte_offset, config.frame_base_byte_anchor);
    validate_policy(displacement, config);
    result.stack_slots.push_back(BaseRelativeBackendFrameSlot{
        slot.stack_slot, slot.byte_offset, slot.byte_size, displacement});
  }

  result.class_storage.reserve(frame.class_storage.size());
  for (const ByteAddressedBackendClassStorage& item : frame.class_storage) {
    result.class_storage.push_back(
        BaseRelativeBackendClassStorage{item.class_id,
                                        map_storage(item.storage,
                                                    result.stack_slots)});
  }
  result.location_storage.reserve(frame.location_storage.size());
  for (const ByteAddressedBackendLocationStorage& item :
       frame.location_storage) {
    result.location_storage.push_back(BaseRelativeBackendLocationStorage{
        item.location, item.class_id,
        map_storage(item.storage, result.stack_slots)});
  }

  result.blocks.reserve(frame.blocks.size());
  for (const ByteAddressedBackendBlock& block : frame.blocks) {
    BaseRelativeBackendBlock mapped_block;
    mapped_block.reachable = block.reachable;
    mapped_block.original_block = block.original_block;
    mapped_block.operations.reserve(block.operations.size());
    for (const ByteAddressedBackendOperation& operation : block.operations) {
      BaseRelativeBackendOperation mapped_operation;
      mapped_operation.kind = operation.kind;
      mapped_operation.origin_kind = operation.origin_kind;
      mapped_operation.origin_index = operation.origin_index;
      mapped_operation.inputs.reserve(operation.inputs.size());
      for (const ByteAddressedBackendStorage input : operation.inputs) {
        mapped_operation.inputs.push_back(
            map_storage(input, result.stack_slots));
      }
      if (operation.output.has_value()) {
        mapped_operation.output =
            map_storage(*operation.output, result.stack_slots);
      }
      mapped_block.operations.push_back(std::move(mapped_operation));
    }
    result.blocks.push_back(std::move(mapped_block));
  }

  return result;
}

}  // namespace algorithms::graphs
