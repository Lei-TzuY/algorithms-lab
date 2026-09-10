#include "algorithms/graphs/backend_spill_lowering.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

[[nodiscard]] bool is_register(const BackendStorage storage) {
  return storage.kind == BackendStorageKind::physical_register ||
         storage.kind == BackendStorageKind::spill_scratch_register;
}

[[nodiscard]] bool contains_location(
    const std::vector<SsaCopyLocation>& locations,
    const SsaCopyLocation location) {
  return std::find(locations.begin(), locations.end(), location) !=
         locations.end();
}

[[nodiscard]] std::size_t class_for_location(
    const PhiFreeCoalescedRegisterAllocation& allocation,
    const SsaCopyLocation location) {
  std::size_t found = allocation.classes.size();
  for (const RegisterCoalescingClass& item : allocation.classes) {
    if (std::find(item.members.begin(), item.members.end(), location) ==
        item.members.end()) {
      continue;
    }
    if (found != allocation.classes.size()) {
      throw std::invalid_argument(
          "backend spill lowering location belongs to multiple classes");
    }
    found = item.class_id;
  }
  if (found == allocation.classes.size()) {
    throw std::invalid_argument(
        "backend spill lowering location is missing a coalescing class");
  }
  return found;
}

[[nodiscard]] const BackendLocationStorage& location_binding(
    const PhiFreeBackendSpillLowering& result,
    const SsaCopyLocation location) {
  const auto found = std::find_if(
      result.location_storage.begin(), result.location_storage.end(),
      [location](const BackendLocationStorage& item) {
        return item.location == location;
      });
  if (found == result.location_storage.end()) {
    throw std::invalid_argument(
        "backend spill lowering program references unknown location");
  }
  return *found;
}

[[nodiscard]] SsaCopyLocation value_location(const SsaValue value) {
  SsaCopyLocation result;
  result.kind = SsaCopyLocationKind::value;
  result.value = value;
  result.temporary = 0U;
  return result;
}

[[nodiscard]] BackendControlTerminator lower_control(
    const SsaLoweredControlTerminator& source,
    const PhiFreeBackendSpillLowering& result) {
  BackendControlTerminator lowered;
  lowered.kind = source.kind;
  lowered.termination = source.termination;
  lowered.jump_target = source.jump_target;
  lowered.nonzero_target = source.nonzero_target;
  lowered.zero_target = source.zero_target;
  if (source.predicate.has_value()) {
    const BackendStorage storage =
        location_binding(result, *source.predicate).storage;
    if (storage.kind == BackendStorageKind::spill_scratch_register) {
      throw std::logic_error(
          "backend control predicate cannot use transient scratch storage");
    }
    lowered.predicate_storage = storage;
  }
  return lowered;
}

void append_operation(
    std::vector<BackendOperation>& output, const BackendOperationKind kind,
    const PhiFreeOperationKind origin_kind, const std::size_t origin_index,
    std::vector<BackendStorage> inputs,
    const std::optional<BackendStorage> result_storage,
    const SsaInstructionSemantics instruction_semantics = {}) {
  output.push_back(BackendOperation{kind, origin_kind, origin_index,
                                    std::move(inputs), result_storage,
                                    instruction_semantics});
}

void lower_move(const SsaScheduledMove& move,
                const PhiFreeOperationKind origin_kind,
                const std::size_t origin_index,
                PhiFreeBackendSpillLowering& result,
                std::vector<BackendOperation>& output) {
  const BackendStorage source =
      location_binding(result, move.source).storage;
  const BackendStorage destination =
      location_binding(result, move.destination).storage;
  if (source == destination) {
    return;
  }

  const bool source_stack = source.kind == BackendStorageKind::stack_slot;
  const bool destination_stack =
      destination.kind == BackendStorageKind::stack_slot;
  if (!source_stack && !destination_stack) {
    if (!is_register(source) || !is_register(destination)) {
      throw std::logic_error("backend register move has invalid storage kind");
    }
    append_operation(output, BackendOperationKind::register_move, origin_kind,
                     origin_index, {source}, destination);
    return;
  }
  if (!source_stack) {
    if (!is_register(source)) {
      throw std::logic_error("backend stack store source is not a register");
    }
    append_operation(output, BackendOperationKind::stack_store, origin_kind,
                     origin_index, {source}, destination);
    return;
  }
  if (!destination_stack) {
    if (!is_register(destination)) {
      throw std::logic_error(
          "backend stack reload destination is not a register");
    }
    append_operation(output, BackendOperationKind::stack_reload, origin_kind,
                     origin_index, {source}, destination);
    return;
  }

  const BackendStorage scratch{BackendStorageKind::spill_scratch_register, 0U};
  append_operation(output, BackendOperationKind::stack_reload, origin_kind,
                   origin_index, {source}, scratch);
  append_operation(output, BackendOperationKind::stack_store, origin_kind,
                   origin_index, {scratch}, destination);
  result.max_scratch_registers =
      std::max(result.max_scratch_registers, std::size_t{1U});
}

struct ScratchBinding {
  std::size_t stack_slot{0U};
  BackendStorage scratch;
};

[[nodiscard]] std::optional<BackendStorage> scratch_for_slot(
    const std::vector<ScratchBinding>& bindings, const std::size_t stack_slot) {
  for (const ScratchBinding& item : bindings) {
    if (item.stack_slot == stack_slot) {
      return item.scratch;
    }
  }
  return std::nullopt;
}

[[nodiscard]] BackendStorage create_scratch(
    std::vector<ScratchBinding>& bindings, const std::size_t stack_slot) {
  const BackendStorage scratch{BackendStorageKind::spill_scratch_register,
                               bindings.size()};
  bindings.push_back(ScratchBinding{stack_slot, scratch});
  return scratch;
}

void lower_instruction(const SsaInstruction& instruction,
                       const std::size_t instruction_index,
                       PhiFreeBackendSpillLowering& result,
                       std::vector<BackendOperation>& output) {
  std::vector<ScratchBinding> scratch_bindings;
  std::vector<BackendStorage> lowered_uses;
  lowered_uses.reserve(instruction.uses.size());

  for (const SsaValue use : instruction.uses) {
    const BackendStorage storage =
        location_binding(result, value_location(use)).storage;
    if (storage.kind != BackendStorageKind::stack_slot) {
      if (!is_register(storage)) {
        throw std::logic_error("backend instruction use is not register storage");
      }
      lowered_uses.push_back(storage);
      continue;
    }

    std::optional<BackendStorage> scratch =
        scratch_for_slot(scratch_bindings, storage.index);
    if (!scratch.has_value()) {
      scratch = create_scratch(scratch_bindings, storage.index);
      append_operation(output, BackendOperationKind::stack_reload,
                       PhiFreeOperationKind::instruction, instruction_index,
                       {storage}, *scratch);
    }
    lowered_uses.push_back(*scratch);
  }

  std::optional<BackendStorage> lowered_definition;
  std::optional<BackendStorage> spilled_definition;
  if (instruction.definition.has_value()) {
    const BackendStorage storage =
        location_binding(result, value_location(*instruction.definition)).storage;
    if (storage.kind == BackendStorageKind::stack_slot) {
      spilled_definition = storage;
      lowered_definition = scratch_for_slot(scratch_bindings, storage.index);
      if (!lowered_definition.has_value()) {
        lowered_definition = create_scratch(scratch_bindings, storage.index);
      }
    } else {
      if (!is_register(storage)) {
        throw std::logic_error(
            "backend instruction definition is not register storage");
      }
      lowered_definition = storage;
    }
  }

  result.max_scratch_registers =
      std::max(result.max_scratch_registers, scratch_bindings.size());
  append_operation(output, BackendOperationKind::instruction,
                   PhiFreeOperationKind::instruction, instruction_index,
                   std::move(lowered_uses), lowered_definition,
                   instruction.semantics);

  if (spilled_definition.has_value()) {
    append_operation(output, BackendOperationKind::stack_store,
                     PhiFreeOperationKind::instruction, instruction_index,
                     {*lowered_definition}, *spilled_definition);
  }
}

void validate_allocation_and_build_storage(
    const PhiFreeCoalescedRegisterAllocation& allocation,
    PhiFreeBackendSpillLowering& result) {
  result.register_budget = allocation.register_budget;
  result.class_storage.reserve(allocation.classes.size());

  std::size_t next_stack_slot = 0U;
  for (std::size_t class_id = 0U; class_id < allocation.classes.size();
       ++class_id) {
    const RegisterCoalescingClass& item = allocation.classes[class_id];
    if (item.class_id != class_id || item.members.empty()) {
      throw std::invalid_argument(
          "backend spill lowering has malformed coalescing classes");
    }
    BackendStorage storage;
    if (item.physical_register.has_value()) {
      if (*item.physical_register >= allocation.register_budget) {
        throw std::invalid_argument(
            "backend spill lowering physical register exceeds budget");
      }
      storage = BackendStorage{BackendStorageKind::physical_register,
                               *item.physical_register};
    } else {
      storage = BackendStorage{BackendStorageKind::stack_slot, next_stack_slot};
      ++next_stack_slot;
    }
    result.class_storage.push_back(BackendClassStorage{class_id, storage});
  }
  result.stack_slot_count = next_stack_slot;

  result.location_storage.reserve(allocation.locations.size());
  for (const SsaCopyLocation location : allocation.locations) {
    const std::size_t class_id = class_for_location(allocation, location);
    result.location_storage.push_back(BackendLocationStorage{
        location, class_id, result.class_storage[class_id].storage});
  }

  for (const RegisterCoalescingClass& item : allocation.classes) {
    for (const SsaCopyLocation member : item.members) {
      if (!contains_location(allocation.locations, member)) {
        throw std::invalid_argument(
            "backend spill lowering class contains unknown location");
      }
    }
  }

  if (allocation.assignments.size() != allocation.locations.size()) {
    throw std::invalid_argument(
        "backend spill lowering assignment count does not match locations");
  }
  for (const BackendLocationStorage& binding : result.location_storage) {
    std::size_t assignment_count = 0U;
    for (const PhysicalRegisterAssignment& assignment : allocation.assignments) {
      if (assignment.location != binding.location) {
        continue;
      }
      ++assignment_count;
      const std::optional<std::size_t> expected =
          allocation.classes[binding.class_id].physical_register;
      if (assignment.physical_register != expected) {
        throw std::invalid_argument(
            "backend spill lowering assignment disagrees with class color");
      }
    }
    if (assignment_count != 1U) {
      throw std::invalid_argument(
          "backend spill lowering location assignment is missing or duplicated");
    }

    const bool expected_spill =
        binding.storage.kind == BackendStorageKind::stack_slot;
    const std::size_t spill_count = static_cast<std::size_t>(std::count(
        allocation.spills.begin(), allocation.spills.end(), binding.location));
    if ((expected_spill && spill_count != 1U) ||
        (!expected_spill && spill_count != 0U)) {
      throw std::invalid_argument(
          "backend spill lowering spill list disagrees with class storage");
    }
  }
}

}  // namespace

PhiFreeBackendSpillLowering lower_phi_free_backend_storage(
    const OutOfSsaProgram& program,
    const PhiFreeCoalescedRegisterAllocation& allocation) {
  if (program.blocks.size() != program.graph.vertex_count()) {
    throw std::invalid_argument(
        "backend spill lowering block count does not match CFG");
  }

  PhiFreeBackendSpillLowering result;
  validate_allocation_and_build_storage(allocation, result);
  result.blocks.resize(program.blocks.size());

  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    const SsaLoweredBlock& source = program.blocks[block];
    BackendLoweredBlock& destination = result.blocks[block];
    destination.reachable = source.reachable;
    destination.original_block = source.original_block;
    destination.control = lower_control(source.control, result);

    for (std::size_t index = 0U; index < source.entry_moves.size(); ++index) {
      lower_move(source.entry_moves[index], PhiFreeOperationKind::entry_move,
                 index, result, destination.operations);
    }
    for (std::size_t index = 0U; index < source.instructions.size(); ++index) {
      lower_instruction(source.instructions[index], index, result,
                        destination.operations);
    }
    for (std::size_t index = 0U; index < source.exit_moves.size(); ++index) {
      lower_move(source.exit_moves[index], PhiFreeOperationKind::exit_move,
                 index, result, destination.operations);
    }
  }

  return result;
}

}  // namespace algorithms::graphs
