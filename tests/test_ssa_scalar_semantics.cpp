#include "algorithms/graphs/ssa_destruction.hpp"
#include "algorithms/graphs/ssa_scalar_semantics.hpp"

#include "test_framework.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using namespace algorithms::graphs;

namespace {

SsaInstruction binary_instruction(const SsaScalarOpcode opcode) {
  return SsaInstruction{{SsaValue{0U, 0U}, SsaValue{1U, 0U}},
                        SsaValue{2U, 1U}, {opcode, std::nullopt}};
}

TEST_CASE(ssa_scalar_semantics_legacy_default_and_shape_validation) {
  const SsaInstruction legacy{{SsaValue{0U, 0U}}, std::nullopt};
  REQUIRE_EQ(legacy.semantics.opcode, SsaScalarOpcode::opaque);
  REQUIRE(!legacy.semantics.immediate.has_value());
  REQUIRE(!ssa_instruction_has_executable_scalar_semantics(legacy));
  REQUIRE_THROWS_AS(evaluate_ssa_scalar_instruction(legacy, std::array<std::int64_t, 1U>{7}),
                    std::invalid_argument);

  REQUIRE_THROWS_AS(
      validate_ssa_instruction_semantics(
          SsaInstruction{{}, SsaValue{0U, 1U},
                         {SsaScalarOpcode::opaque, std::int64_t{1}}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      validate_ssa_instruction_semantics(
          SsaInstruction{{SsaValue{0U, 0U}}, SsaValue{1U, 1U},
                         {SsaScalarOpcode::constant_i64, std::int64_t{1}}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      validate_ssa_instruction_semantics(
          SsaInstruction{{}, std::nullopt,
                         {SsaScalarOpcode::constant_i64, std::int64_t{1}}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      validate_ssa_instruction_semantics(
          SsaInstruction{{SsaValue{0U, 0U}}, SsaValue{1U, 1U},
                         {SsaScalarOpcode::copy_i64, std::int64_t{1}}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      validate_ssa_instruction_semantics(
          SsaInstruction{{SsaValue{0U, 0U}}, SsaValue{1U, 1U},
                         {SsaScalarOpcode::add_i64, std::nullopt}}),
      std::invalid_argument);
}

TEST_CASE(ssa_scalar_semantics_checked_deterministic_evaluation) {
  const SsaInstruction constant{{}, SsaValue{0U, 1U},
                                {SsaScalarOpcode::constant_i64,
                                 std::numeric_limits<std::int64_t>::min()}};
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(constant, {}),
             std::numeric_limits<std::int64_t>::min());

  const SsaInstruction copy{{SsaValue{0U, 0U}}, SsaValue{1U, 1U},
                            {SsaScalarOpcode::copy_i64, std::nullopt}};
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 copy, std::array<std::int64_t, 1U>{-19}),
             -19);

  const std::array<std::int64_t, 2U> operands{7, -3};
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 binary_instruction(SsaScalarOpcode::add_i64), operands),
             4);
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 binary_instruction(SsaScalarOpcode::subtract_i64), operands),
             10);
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 binary_instruction(SsaScalarOpcode::multiply_i64), operands),
             -21);
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 binary_instruction(SsaScalarOpcode::equal_i64), operands),
             0);
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 binary_instruction(SsaScalarOpcode::less_than_i64), operands),
             0);

  const auto maximum = std::numeric_limits<std::int64_t>::max();
  const auto minimum = std::numeric_limits<std::int64_t>::min();
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 binary_instruction(SsaScalarOpcode::add_i64),
                 std::array<std::int64_t, 2U>{maximum - 1, 1}),
             maximum);
  REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                 binary_instruction(SsaScalarOpcode::subtract_i64),
                 std::array<std::int64_t, 2U>{minimum + 1, 1}),
             minimum);
  REQUIRE_THROWS_AS(
      evaluate_ssa_scalar_instruction(binary_instruction(SsaScalarOpcode::add_i64),
                                      std::array<std::int64_t, 2U>{maximum, 1}),
      std::overflow_error);
  REQUIRE_THROWS_AS(
      evaluate_ssa_scalar_instruction(binary_instruction(SsaScalarOpcode::subtract_i64),
                                      std::array<std::int64_t, 2U>{minimum, 1}),
      std::overflow_error);
  REQUIRE_THROWS_AS(
      evaluate_ssa_scalar_instruction(binary_instruction(SsaScalarOpcode::multiply_i64),
                                      std::array<std::int64_t, 2U>{minimum, -1}),
      std::overflow_error);
  REQUIRE_THROWS_AS(
      evaluate_ssa_scalar_instruction(binary_instruction(SsaScalarOpcode::multiply_i64),
                                      std::array<std::int64_t, 2U>{maximum, 2}),
      std::overflow_error);
}

TEST_CASE(ssa_scalar_semantics_randomized_small_domain_differential) {
  std::mt19937_64 random(0x53534153454D3636ULL);
  for (int trial = 0; trial < 5000; ++trial) {
    const std::int64_t left =
        static_cast<std::int64_t>(random() % 2000001ULL) - 1000000;
    const std::int64_t right =
        static_cast<std::int64_t>(random() % 2000001ULL) - 1000000;
    const std::array<std::int64_t, 2U> operands{left, right};

    REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                   binary_instruction(SsaScalarOpcode::add_i64), operands),
               left + right);
    REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                   binary_instruction(SsaScalarOpcode::subtract_i64), operands),
               left - right);
    REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                   binary_instruction(SsaScalarOpcode::multiply_i64), operands),
               left * right);
    REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                   binary_instruction(SsaScalarOpcode::equal_i64), operands),
               left == right ? 1 : 0);
    REQUIRE_EQ(evaluate_ssa_scalar_instruction(
                   binary_instruction(SsaScalarOpcode::less_than_i64), operands),
               left < right ? 1 : 0);
  }
}

TEST_CASE(ssa_scalar_semantics_construct_and_destroy_preserve_descriptor) {
  Graph graph(1U, true);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(1U);
  blocks[0U].push_back(
      SemanticSsaInputInstruction{{}, Variable{0U},
                                  {SsaScalarOpcode::constant_i64, 11}});
  blocks[0U].push_back(
      SemanticSsaInputInstruction{{0U}, Variable{1U},
                                  {SsaScalarOpcode::copy_i64, std::nullopt}});
  blocks[0U].push_back(
      SemanticSsaInputInstruction{{0U, 1U}, Variable{2U},
                                  {SsaScalarOpcode::add_i64, std::nullopt}});

  const SsaProgram program = construct_semantic_ssa(graph, 0U, 3U, blocks);
  REQUIRE_EQ(program.blocks[0U].instructions.size(), 3U);
  for (std::size_t index = 0U; index < blocks[0U].size(); ++index) {
    REQUIRE(program.blocks[0U].instructions[index].semantics ==
            blocks[0U][index].semantics);
  }
  REQUIRE_EQ(program.blocks[0U].instructions[2U].uses.size(), 2U);
  REQUIRE(program.blocks[0U].instructions[2U].definition.has_value());

  const OutOfSsaProgram lowered = destroy_ssa(graph, program);
  REQUIRE_EQ(lowered.blocks[0U].instructions.size(), 3U);
  for (std::size_t index = 0U; index < blocks[0U].size(); ++index) {
    REQUIRE(lowered.blocks[0U].instructions[index].semantics ==
            blocks[0U][index].semantics);
  }

  std::vector<std::vector<SsaInputInstruction>> legacy_blocks(1U);
  legacy_blocks[0U].push_back(SsaInputInstruction{{0U}, Variable{1U}});
  const SsaProgram legacy = construct_ssa(graph, 0U, 2U, legacy_blocks);
  REQUIRE_EQ(legacy.blocks[0U].instructions[0U].semantics.opcode,
             SsaScalarOpcode::opaque);
}

TEST_CASE(ssa_scalar_semantics_constructor_rejects_malformed_shapes) {
  Graph graph(1U, true);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(1U);
  blocks[0U].push_back(
      SemanticSsaInputInstruction{{0U}, Variable{1U},
                                  {SsaScalarOpcode::constant_i64, 7}});
  REQUIRE_THROWS_AS(construct_semantic_ssa(graph, 0U, 2U, blocks),
                    std::invalid_argument);

  blocks[0U][0U] =
      SemanticSsaInputInstruction{{0U, 1U}, Variable{1U},
                                  {SsaScalarOpcode::add_i64, std::nullopt}};
  REQUIRE_THROWS_AS(construct_semantic_ssa(graph, 0U, 1U, blocks),
                    std::out_of_range);
}

}  // namespace
