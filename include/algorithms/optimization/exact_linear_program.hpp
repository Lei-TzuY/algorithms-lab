#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::optimization {

struct Rational64 {
  std::int64_t numerator = 0;
  std::int64_t denominator = 1;

  friend bool operator==(const Rational64&, const Rational64&) = default;
};

enum class LinearProgramStatus {
  Optimal,
  Infeasible,
  Unbounded,
};

struct LinearProgramResult {
  LinearProgramStatus status = LinearProgramStatus::Infeasible;
  Rational64 objective{};
  std::vector<Rational64> variables;
  std::vector<Rational64> unbounded_direction;
  bool phase_one_used = false;
  std::size_t pivot_count = 0;
};

// Solves the canonical linear program
//
//   maximize     objective^T x
//   subject to   coefficients * x <= bounds
//                x >= 0
//
// Input coefficients are exact signed 64-bit integers. All tableau arithmetic
// is exact reduced-rational arithmetic. If a required exact intermediate cannot
// be represented by Rational64, the solver throws std::overflow_error instead
// of silently rounding or wrapping.
[[nodiscard]] LinearProgramResult maximize_linear_program(
    const std::vector<std::vector<std::int64_t>>& coefficients,
    const std::vector<std::int64_t>& bounds,
    const std::vector<std::int64_t>& objective);

}  // namespace algorithms::optimization
