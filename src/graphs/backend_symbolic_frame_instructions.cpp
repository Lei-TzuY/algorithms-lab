#include "algorithms/graphs/backend_symbolic_frame_instructions.hpp"

#include <stdexcept>
#include <variant>

namespace algorithms::graphs {
namespace {

BackendSymbolicFixedFrameInstruction lower_action(
    const BackendFixedFrameAction& action) {
  if (const auto* adjustment =
          std::get_if<BackendStackPointerAdjustmentAction>(&action)) {
    switch (adjustment->direction) {
      case BackendStackPointerMoveDirection::toward_lower_addresses:
        return BackendStackPointerDecrementImmediateInstruction{
            adjustment->stack_pointer_physical_register,
            adjustment->byte_count};
      case BackendStackPointerMoveDirection::toward_higher_addresses:
        return BackendStackPointerIncrementImmediateInstruction{
            adjustment->stack_pointer_physical_register,
            adjustment->byte_count};
    }
    throw std::logic_error(
        "symbolic frame lowering received an invalid stack-move direction");
  }

  if (const auto* materialization =
          std::get_if<BackendFrameBaseMaterializationAction>(&action)) {
    return BackendFrameBaseFromStackPointerImmediateInstruction{
        materialization->frame_base_physical_register,
        materialization->stack_pointer_physical_register,
        materialization->displacement_from_adjusted_stack_pointer};
  }

  throw std::logic_error(
      "symbolic frame lowering received an unsupported Phase-58 action");
}

}  // namespace

BackendSymbolicFixedFrameInstructionPlan
lower_fixed_frame_actions_to_symbolic_instructions(
    const LegalizedBackendFixedFrameActionPlan& source_plan) {
  const LegalizedBackendFixedFrameActionPlan canonical =
      legalize_fixed_frame_actions(source_plan.source_plan, source_plan.policy);
  if (canonical != source_plan) {
    throw std::logic_error(
        "symbolic frame lowering requires a canonical Phase-58 plan");
  }

  BackendSymbolicFixedFrameInstructionPlan result;
  result.source_plan = source_plan;
  result.entry_instructions.reserve(source_plan.entry_actions.size());
  result.exit_instructions.reserve(source_plan.exit_actions.size());

  for (const BackendFixedFrameAction& action : source_plan.entry_actions) {
    result.entry_instructions.push_back(lower_action(action));
  }
  for (const BackendFixedFrameAction& action : source_plan.exit_actions) {
    result.exit_instructions.push_back(lower_action(action));
  }
  return result;
}

}  // namespace algorithms::graphs
