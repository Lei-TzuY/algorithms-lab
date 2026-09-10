#include "algorithms/graphs/backend_frame_action_legalization.hpp"

#include <cstddef>
#include <stdexcept>
#include <variant>

namespace algorithms::graphs {
namespace {

void validate_policy(const BackendFixedFrameActionLegalityPolicy& policy) {
  if (policy.maximum_stack_adjustment_immediate_magnitude == 0U) {
    throw std::invalid_argument(
        "backend frame action stack-adjustment immediate magnitude must be positive");
  }
  if (policy.minimum_frame_base_immediate >
      policy.maximum_frame_base_immediate) {
    throw std::invalid_argument(
        "backend frame-base immediate interval is inverted");
  }
}

std::size_t required_chunk_count(const std::size_t magnitude,
                                 const std::size_t maximum_chunk) {
  if (magnitude == 0U) {
    return 1U;
  }
  const std::size_t quotient = magnitude / maximum_chunk;
  const std::size_t remainder = magnitude % maximum_chunk;
  return quotient + (remainder == 0U ? 0U : 1U);
}

void validate_output_capacity(const std::size_t chunk_count,
                              const LegalizedBackendFixedFrameActionPlan& result) {
  if (result.entry_actions.max_size() == 0U ||
      chunk_count > result.entry_actions.max_size() - 1U ||
      chunk_count > result.exit_actions.max_size()) {
    throw std::length_error(
        "legalized backend frame action sequence exceeds vector capacity");
  }
}

void append_entry_chunks(
    std::vector<BackendFixedFrameAction>& actions,
    const BackendStackPointerAdjustmentAction& original,
    const std::size_t maximum_chunk) {
  if (original.byte_count == 0U) {
    actions.push_back(BackendStackPointerAdjustmentAction{
        original.stack_pointer_physical_register, original.direction, 0U});
    return;
  }

  std::size_t remaining = original.byte_count;
  while (remaining > maximum_chunk) {
    actions.push_back(BackendStackPointerAdjustmentAction{
        original.stack_pointer_physical_register, original.direction,
        maximum_chunk});
    remaining -= maximum_chunk;
  }
  actions.push_back(BackendStackPointerAdjustmentAction{
      original.stack_pointer_physical_register, original.direction, remaining});
}

void append_inverse_exit_chunks(
    std::vector<BackendFixedFrameAction>& actions,
    const BackendStackPointerAdjustmentAction& original,
    const std::size_t maximum_chunk) {
  if (original.byte_count == 0U) {
    actions.push_back(BackendStackPointerAdjustmentAction{
        original.stack_pointer_physical_register, original.direction, 0U});
    return;
  }

  const std::size_t quotient = original.byte_count / maximum_chunk;
  const std::size_t remainder = original.byte_count % maximum_chunk;
  if (remainder != 0U) {
    actions.push_back(BackendStackPointerAdjustmentAction{
        original.stack_pointer_physical_register, original.direction, remainder});
  }
  for (std::size_t index = 0U; index < quotient; ++index) {
    actions.push_back(BackendStackPointerAdjustmentAction{
        original.stack_pointer_physical_register, original.direction,
        maximum_chunk});
  }
}

}  // namespace

LegalizedBackendFixedFrameActionPlan legalize_fixed_frame_actions(
    const BackendFixedFrameActionPlan& source_plan,
    const BackendFixedFrameActionLegalityPolicy policy) {
  validate_policy(policy);

  const BackendFixedFrameActionPlan canonical =
      derive_fixed_frame_actions(source_plan.source_plan);
  if (canonical != source_plan) {
    throw std::logic_error(
        "backend frame action legalization requires a canonical Phase-57 plan");
  }

  LegalizedBackendFixedFrameActionPlan result;
  result.source_plan = source_plan;
  result.policy = policy;

  if (source_plan.entry_actions.empty()) {
    if (!source_plan.exit_actions.empty()) {
      throw std::logic_error(
          "empty Phase-57 entry action sequence has nonempty exit actions");
    }
    return result;
  }

  if (source_plan.entry_actions.size() != 2U ||
      source_plan.exit_actions.size() != 1U) {
    throw std::logic_error("unexpected canonical Phase-57 action shape");
  }

  const auto* entry_adjustment =
      std::get_if<BackendStackPointerAdjustmentAction>(
          &source_plan.entry_actions[0]);
  const auto* materialization =
      std::get_if<BackendFrameBaseMaterializationAction>(
          &source_plan.entry_actions[1]);
  const auto* exit_adjustment =
      std::get_if<BackendStackPointerAdjustmentAction>(
          &source_plan.exit_actions[0]);
  if (entry_adjustment == nullptr || materialization == nullptr ||
      exit_adjustment == nullptr) {
    throw std::logic_error("unexpected canonical Phase-57 action variants");
  }

  if (materialization->displacement_from_adjusted_stack_pointer <
          policy.minimum_frame_base_immediate ||
      materialization->displacement_from_adjusted_stack_pointer >
          policy.maximum_frame_base_immediate) {
    throw std::out_of_range(
        "backend frame-base materialization displacement is not immediate-legal");
  }

  const std::size_t chunk_count = required_chunk_count(
      entry_adjustment->byte_count,
      policy.maximum_stack_adjustment_immediate_magnitude);
  validate_output_capacity(chunk_count, result);
  result.entry_actions.reserve(chunk_count + 1U);
  result.exit_actions.reserve(chunk_count);

  append_entry_chunks(result.entry_actions, *entry_adjustment,
                      policy.maximum_stack_adjustment_immediate_magnitude);
  result.entry_actions.push_back(*materialization);
  append_inverse_exit_chunks(result.exit_actions, *exit_adjustment,
                             policy.maximum_stack_adjustment_immediate_magnitude);
  return result;
}

}  // namespace algorithms::graphs
