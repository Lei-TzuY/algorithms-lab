#include "algorithms/graphs/backend_frame_layout.hpp"

#include <cstddef>
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
    throw std::overflow_error("backend frame byte arithmetic overflow");
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

void validate_config(const BackendFrameLayoutConfig config) {
  if (config.stack_slot_size_bytes == 0U) {
    throw std::invalid_argument("backend frame stack-slot size must be nonzero");
  }
  if (!is_power_of_two(config.stack_slot_alignment_bytes)) {
    throw std::invalid_argument(
        "backend frame stack-slot alignment must be a power of two");
  }
  if (!is_power_of_two(config.frame_alignment_bytes)) {
    throw std::invalid_argument(
        "backend frame alignment must be a power of two");
  }
  if (config.frame_alignment_bytes < config.stack_slot_alignment_bytes) {
    throw std::invalid_argument(
        "backend frame alignment must cover stack-slot alignment");
  }
}

[[nodiscard]] std::vector<BackendFrameSlotLayout> layout_slots(
    const std::size_t stack_slot_count, const BackendFrameLayoutConfig config,
    std::size_t& frame_size_bytes) {
  std::vector<BackendFrameSlotLayout> slots;
  slots.reserve(stack_slot_count);
  std::size_t cursor = 0U;
  for (std::size_t slot = 0U; slot < stack_slot_count; ++slot) {
    const std::size_t offset =
        checked_align_up(cursor, config.stack_slot_alignment_bytes);
    cursor = checked_add(offset, config.stack_slot_size_bytes);
    slots.push_back(
        BackendFrameSlotLayout{slot, offset, config.stack_slot_size_bytes});
  }
  frame_size_bytes = checked_align_up(cursor, config.frame_alignment_bytes);
  return slots;
}

[[nodiscard]] ByteAddressedBackendStorage map_persistent_storage(
    const BackendStorage storage,
    const std::vector<BackendFrameSlotLayout>& stack_slots,
    const std::size_t allocatable_registers) {
  if (storage.kind == BackendStorageKind::physical_register) {
    if (storage.index >= allocatable_registers) {
      throw std::logic_error(
          "backend frame persistent register exceeds allocatable prefix");
    }
    return {ByteAddressedBackendStorageKind::physical_register, storage.index};
  }
  if (storage.kind == BackendStorageKind::stack_slot) {
    if (storage.index >= stack_slots.size()) {
      throw std::logic_error("backend frame persistent stack slot out of range");
    }
    return {ByteAddressedBackendStorageKind::frame_byte_offset,
            stack_slots[storage.index].byte_offset};
  }
  throw std::logic_error(
      "backend frame persistent storage contains abstract scratch register");
}

[[nodiscard]] ByteAddressedBackendStorage map_operation_storage(
    const BackendStorage storage,
    const std::vector<BackendFrameSlotLayout>& stack_slots,
    const std::size_t total_registers) {
  if (storage.kind == BackendStorageKind::physical_register) {
    if (storage.index >= total_registers) {
      throw std::logic_error(
          "backend frame operation register exceeds physical register file");
    }
    return {ByteAddressedBackendStorageKind::physical_register, storage.index};
  }
  if (storage.kind == BackendStorageKind::stack_slot) {
    if (storage.index >= stack_slots.size()) {
      throw std::logic_error("backend frame operation stack slot out of range");
    }
    return {ByteAddressedBackendStorageKind::frame_byte_offset,
            stack_slots[storage.index].byte_offset};
  }
  throw std::logic_error(
      "backend frame physicalized operation contains abstract scratch register");
}

}  // namespace

ScratchAwareByteAddressedBackendFrame layout_scratch_aware_backend_frame(
    const ScratchAwareBackendRegisterSelection& selection,
    const BackendFrameLayoutConfig config) {
  validate_config(config);
  const std::size_t total_registers = checked_add(
      selection.allocatable_registers, selection.reserved_scratch_registers);

  if (selection.physicalized_blocks.size() !=
      selection.abstract_lowering.blocks.size()) {
    throw std::logic_error(
        "backend frame physicalized block count is inconsistent");
  }
  for (std::size_t block = 0U; block < selection.physicalized_blocks.size();
       ++block) {
    const BackendLoweredBlock& abstract_block =
        selection.abstract_lowering.blocks[block];
    const BackendLoweredBlock& physical_block =
        selection.physicalized_blocks[block];
    if (abstract_block.reachable != physical_block.reachable ||
        abstract_block.original_block != physical_block.original_block ||
        abstract_block.operations.size() != physical_block.operations.size()) {
      throw std::logic_error(
          "backend frame physicalized block provenance is inconsistent");
    }
    for (std::size_t operation = 0U;
         operation < abstract_block.operations.size(); ++operation) {
      const BackendOperation& before = abstract_block.operations[operation];
      const BackendOperation& after = physical_block.operations[operation];
      if (before.kind != after.kind || before.origin_kind != after.origin_kind ||
          before.origin_index != after.origin_index ||
          before.instruction_semantics != after.instruction_semantics ||
          before.inputs.size() != after.inputs.size() ||
          before.output.has_value() != after.output.has_value()) {
        throw std::logic_error(
            "backend frame physicalized operation provenance is inconsistent");
      }
    }
  }

  if (selection.scratch_physical_registers.size() !=
      selection.abstract_lowering.max_scratch_registers) {
    throw std::logic_error(
        "backend frame scratch-register witness size is inconsistent");
  }
  for (std::size_t scratch = 0U;
       scratch < selection.scratch_physical_registers.size(); ++scratch) {
    const std::size_t expected =
        checked_add(selection.allocatable_registers, scratch);
    if (selection.scratch_physical_registers[scratch] != expected ||
        expected >= total_registers) {
      throw std::logic_error(
          "backend frame scratch-register witness is inconsistent");
    }
  }

  ScratchAwareByteAddressedBackendFrame result;
  result.config = config;
  result.total_registers = total_registers;
  result.reserved_scratch_registers = selection.reserved_scratch_registers;
  result.allocatable_registers = selection.allocatable_registers;
  result.scratch_physical_registers = selection.scratch_physical_registers;
  result.stack_slots = layout_slots(selection.abstract_lowering.stack_slot_count,
                                    config, result.frame_size_bytes);

  result.class_storage.reserve(selection.abstract_lowering.class_storage.size());
  for (const BackendClassStorage& item :
       selection.abstract_lowering.class_storage) {
    result.class_storage.push_back(ByteAddressedBackendClassStorage{
        item.class_id,
        map_persistent_storage(item.storage, result.stack_slots,
                               selection.allocatable_registers)});
  }

  result.location_storage.reserve(
      selection.abstract_lowering.location_storage.size());
  for (const BackendLocationStorage& item :
       selection.abstract_lowering.location_storage) {
    result.location_storage.push_back(ByteAddressedBackendLocationStorage{
        item.location, item.class_id,
        map_persistent_storage(item.storage, result.stack_slots,
                               selection.allocatable_registers)});
  }

  result.blocks.reserve(selection.physicalized_blocks.size());
  for (const BackendLoweredBlock& block : selection.physicalized_blocks) {
    ByteAddressedBackendBlock mapped_block;
    mapped_block.reachable = block.reachable;
    mapped_block.original_block = block.original_block;
    mapped_block.operations.reserve(block.operations.size());
    for (const BackendOperation& operation : block.operations) {
      ByteAddressedBackendOperation mapped_operation;
      mapped_operation.kind = operation.kind;
      mapped_operation.origin_kind = operation.origin_kind;
      mapped_operation.origin_index = operation.origin_index;
      mapped_operation.instruction_semantics = operation.instruction_semantics;
      mapped_operation.inputs.reserve(operation.inputs.size());
      for (const BackendStorage input : operation.inputs) {
        mapped_operation.inputs.push_back(map_operation_storage(
            input, result.stack_slots, total_registers));
      }
      if (operation.output.has_value()) {
        mapped_operation.output = map_operation_storage(
            *operation.output, result.stack_slots, total_registers);
      }
      mapped_block.operations.push_back(std::move(mapped_operation));
    }
    result.blocks.push_back(std::move(mapped_block));
  }

  return result;
}

}  // namespace algorithms::graphs
