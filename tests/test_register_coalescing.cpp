#include "algorithms/graphs/register_coalescing.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

struct LocationLess {
  bool operator()(const SsaCopyLocation& a, const SsaCopyLocation& b) const {
    if (a.kind != b.kind) return a.kind == SsaCopyLocationKind::value;
    if (a.kind == SsaCopyLocationKind::value) {
      return std::tie(a.value.variable, a.value.version) <
             std::tie(b.value.variable, b.value.version);
    }
    return a.temporary < b.temporary;
  }
};

OutOfSsaProgram one_block(const std::size_t variables) {
  OutOfSsaProgram program;
  program.start = 0U;
  program.variable_count = variables;
  program.graph = Graph(1U, true);
  program.blocks.resize(1U);
  program.blocks[0].reachable = true;
  for (std::size_t variable = 0U; variable < variables; ++variable) {
    program.initial_values.push_back({variable, 0U});
  }
  return program;
}

SsaCopyLocation loc(const std::size_t variable) {
  return SsaCopyLocation::from_value({variable, 0U});
}

std::size_t class_of(const PhiFreeCoalescedRegisterAllocation& result,
                     const SsaCopyLocation location) {
  for (const RegisterCoalescingClass& item : result.classes) {
    if (std::find(item.members.begin(), item.members.end(), location) !=
        item.members.end()) {
      return item.class_id;
    }
  }
  throw std::runtime_error("coalescing class missing");
}

std::optional<std::size_t> reg_of(
    const PhiFreeCoalescedRegisterAllocation& result,
    const SsaCopyLocation location) {
  for (const PhysicalRegisterAssignment& item : result.assignments) {
    if (item.location == location) {
      return item.physical_register;
    }
  }
  throw std::runtime_error("coalescing assignment missing");
}

bool original_interferes(const PhiFreeCoalescedRegisterAllocation& result,
                         const std::size_t first_class,
                         const std::size_t second_class) {
  for (const RegisterInterferenceEdge& edge :
       result.original_interference_edges) {
    const std::size_t first = class_of(result, edge.first);
    const std::size_t second = class_of(result, edge.second);
    if ((first == first_class && second == second_class) ||
        (first == second_class && second == first_class)) {
      return true;
    }
  }
  return false;
}

std::vector<SsaScheduledMove> all_moves(const OutOfSsaProgram& program) {
  std::vector<SsaScheduledMove> result;
  for (const SsaLoweredBlock& block : program.blocks) {
    result.insert(result.end(), block.entry_moves.begin(),
                  block.entry_moves.end());
    result.insert(result.end(), block.exit_moves.begin(), block.exit_moves.end());
  }
  return result;
}

void verify_result(const OutOfSsaProgram& program,
                   const PhiFreeCoalescedRegisterAllocation& result) {
  std::vector<SsaCopyLocation> members;
  for (std::size_t class_id = 0U; class_id < result.classes.size(); ++class_id) {
    REQUIRE_EQ(result.classes[class_id].class_id, class_id);
    REQUIRE(!result.classes[class_id].members.empty());
    for (const SsaCopyLocation member : result.classes[class_id].members) {
      members.push_back(member);
    }
  }
  std::sort(members.begin(), members.end(), LocationLess{});
  REQUIRE_EQ(members, result.locations);

  for (const RegisterInterferenceEdge& edge :
       result.original_interference_edges) {
    REQUIRE(class_of(result, edge.first) != class_of(result, edge.second));
  }
  for (const QuotientInterferenceEdge& edge :
       result.quotient_interference_edges) {
    REQUIRE(edge.first_class < result.classes.size());
    REQUIRE(edge.second_class < result.classes.size());
    REQUIRE(edge.first_class < edge.second_class);
    REQUIRE(original_interferes(result, edge.first_class, edge.second_class));
    const auto first = result.classes[edge.first_class].physical_register;
    const auto second = result.classes[edge.second_class].physical_register;
    if (first.has_value() && second.has_value()) {
      REQUIRE(*first != *second);
    }
  }

  REQUIRE_EQ(result.assignments.size(), result.locations.size());
  for (const RegisterCoalescingClass& item : result.classes) {
    for (const SsaCopyLocation member : item.members) {
      REQUIRE_EQ(reg_of(result, member), item.physical_register);
    }
  }

  const std::vector<SsaScheduledMove> moves = all_moves(program);
  REQUIRE_EQ(result.preferences.size(), moves.size());
  std::size_t redundant = 0U;
  for (std::size_t index = 0U; index < moves.size(); ++index) {
    REQUIRE_EQ(result.preferences[index].move, moves[index]);
    const std::size_t first = class_of(result, moves[index].source);
    const std::size_t second = class_of(result, moves[index].destination);
    if (first == second) {
      ++redundant;
      REQUIRE(result.preferences[index].decision !=
              CopyCoalescingDecisionKind::blocked_by_interference);
    } else {
      REQUIRE_EQ(result.preferences[index].decision,
                 CopyCoalescingDecisionKind::blocked_by_interference);
      REQUIRE(original_interferes(result, first, second));
    }
  }
  REQUIRE_EQ(result.redundant_copies.size(), redundant);
  for (const RedundantScheduledCopy& copy : result.redundant_copies) {
    REQUIRE(class_of(result, copy.move.source) ==
            class_of(result, copy.move.destination));
  }
}

TEST_CASE(copy_coalescing_safe_move_becomes_redundant) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].entry_moves.push_back({loc(1U), loc(0U)});

  const auto result = coalesce_phi_free_registers(program, 1U);
  verify_result(program, result);
  REQUIRE_EQ(result.classes.size(), 1U);
  REQUIRE_EQ(result.redundant_copies.size(), 1U);
  REQUIRE_EQ(result.preferences[0].decision,
             CopyCoalescingDecisionKind::merged);
  REQUIRE_EQ(reg_of(result, loc(0U)), reg_of(result, loc(1U)));
}

TEST_CASE(copy_coalescing_blocks_interfering_move) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].entry_moves.push_back({loc(1U), loc(0U)});
  program.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});

  const auto result = coalesce_phi_free_registers(program, 2U);
  verify_result(program, result);
  REQUIRE_EQ(result.classes.size(), 2U);
  REQUIRE_EQ(result.preferences[0].decision,
             CopyCoalescingDecisionKind::blocked_by_interference);
  REQUIRE(reg_of(result, loc(0U)).has_value());
  REQUIRE(reg_of(result, loc(1U)).has_value());
  REQUIRE(*reg_of(result, loc(0U)) != *reg_of(result, loc(1U)));
}

TEST_CASE(copy_coalescing_checks_merged_class_against_interference) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].entry_moves.push_back({loc(2U), loc(1U)});
  program.blocks[0].entry_moves.push_back({loc(1U), loc(0U)});
  program.blocks[0].instructions.push_back(
      {{{0U, 0U}, {2U, 0U}}, std::nullopt});

  const auto result = coalesce_phi_free_registers(program, 3U);
  verify_result(program, result);
  REQUIRE_EQ(result.preferences[0].decision,
             CopyCoalescingDecisionKind::merged);
  REQUIRE_EQ(result.preferences[1].decision,
             CopyCoalescingDecisionKind::blocked_by_interference);
  REQUIRE_EQ(class_of(result, loc(1U)), class_of(result, loc(2U)));
  REQUIRE(class_of(result, loc(0U)) != class_of(result, loc(1U)));
}

TEST_CASE(copy_coalescing_zero_budget_spills_whole_class) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].entry_moves.push_back({loc(1U), loc(0U)});
  program.blocks[0].exit_moves.push_back({loc(2U), loc(1U)});

  const auto result = coalesce_phi_free_registers(program, 0U);
  verify_result(program, result);
  REQUIRE_EQ(result.classes.size(), 1U);
  REQUIRE_EQ(result.spills.size(), 3U);
  for (const RegisterCoalescingClass& item : result.classes) {
    REQUIRE(!item.physical_register.has_value());
  }
}

TEST_CASE(copy_coalescing_randomized_invariants_and_determinism) {
  std::mt19937_64 random(0xC0A1E5CEULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t count =
        1U + static_cast<std::size_t>(random() % 8U);
    OutOfSsaProgram program = one_block(count);

    const std::size_t entry_count =
        static_cast<std::size_t>(random() % 7U);
    const std::size_t exit_count =
        static_cast<std::size_t>(random() % 5U);
    for (std::size_t index = 0U; index < entry_count; ++index) {
      program.blocks[0].entry_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }

    const std::size_t instruction_count =
        1U + static_cast<std::size_t>(random() % 4U);
    for (std::size_t index = 0U; index < instruction_count; ++index) {
      SsaInstruction instruction;
      for (std::size_t variable = 0U; variable < count; ++variable) {
        if ((random() & 3U) == 0U) {
          instruction.uses.push_back({variable, 0U});
        }
      }
      if ((random() & 3U) == 0U) {
        instruction.definition =
            SsaValue{static_cast<std::size_t>(random() % count), 0U};
      }
      program.blocks[0].instructions.push_back(std::move(instruction));
    }

    for (std::size_t index = 0U; index < exit_count; ++index) {
      program.blocks[0].exit_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }

    const std::size_t budget =
        static_cast<std::size_t>(random() % 5U);
    const auto result = coalesce_phi_free_registers(program, budget);
    verify_result(program, result);
    REQUIRE_EQ(coalesce_phi_free_registers(program, budget), result);
  }
}

}  // namespace
