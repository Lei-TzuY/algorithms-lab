#include "algorithms/optimization/exact_linear_program.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::optimization {
namespace {

using I64 = std::int64_t;
using U64 = std::uint64_t;

[[nodiscard]] U64 magnitude(I64 value) noexcept {
  if (value >= 0) {
    return static_cast<U64>(value);
  }
  return static_cast<U64>(-(value + 1)) + U64{1};
}

[[nodiscard]] I64 signed_from_magnitude(U64 value, bool negative) {
  const U64 positive_limit = static_cast<U64>(std::numeric_limits<I64>::max());
  const U64 negative_limit = positive_limit + U64{1};
  if (!negative) {
    if (value > positive_limit) {
      throw std::overflow_error("exact rational numerator overflow");
    }
    return static_cast<I64>(value);
  }
  if (value > negative_limit) {
    throw std::overflow_error("exact rational numerator overflow");
  }
  if (value == negative_limit) {
    return std::numeric_limits<I64>::min();
  }
  return -static_cast<I64>(value);
}

[[nodiscard]] I64 checked_add(I64 first, I64 second) {
  if (second > 0 && first > std::numeric_limits<I64>::max() - second) {
    throw std::overflow_error("exact rational addition overflow");
  }
  if (second < 0 && first < std::numeric_limits<I64>::min() - second) {
    throw std::overflow_error("exact rational addition overflow");
  }
  return static_cast<I64>(first + second);
}

[[nodiscard]] I64 checked_negate(I64 value) {
  if (value == std::numeric_limits<I64>::min()) {
    throw std::overflow_error("exact rational negation overflow");
  }
  return static_cast<I64>(-value);
}

[[nodiscard]] I64 checked_multiply(I64 first, I64 second) {
  const bool negative = (first < 0) != (second < 0);
  const U64 first_mag = magnitude(first);
  const U64 second_mag = magnitude(second);
  const U64 positive_limit = static_cast<U64>(std::numeric_limits<I64>::max());
  const U64 limit = negative ? positive_limit + U64{1} : positive_limit;
  if (first_mag != 0 && second_mag > limit / first_mag) {
    throw std::overflow_error("exact rational multiplication overflow");
  }
  return signed_from_magnitude(first_mag * second_mag, negative);
}

[[nodiscard]] I64 divide_signed_by_magnitude(I64 value, U64 divisor) {
  if (divisor == 0) {
    throw std::logic_error("zero exact divisor");
  }
  const U64 signed_limit = static_cast<U64>(std::numeric_limits<I64>::max());
  if (divisor <= signed_limit) {
    return value / static_cast<I64>(divisor);
  }
  if (divisor == signed_limit + U64{1} &&
      value == std::numeric_limits<I64>::min()) {
    return -1;
  }
  throw std::logic_error("exact divisor does not divide signed value");
}

[[nodiscard]] I64 checked_positive_multiply(I64 first, I64 second) {
  if (first <= 0 || second <= 0) {
    throw std::logic_error("positive denominator invariant violated");
  }
  return checked_multiply(first, second);
}

struct Fraction {
  I64 numerator = 0;
  I64 denominator = 1;

  Fraction() = default;
  explicit Fraction(I64 integer) : numerator(integer), denominator(1) {}
  Fraction(I64 num, I64 den) : numerator(num), denominator(den) { normalize(); }

  void normalize() {
    if (denominator <= 0) {
      if (denominator == std::numeric_limits<I64>::min()) {
        throw std::overflow_error("exact rational denominator overflow");
      }
      numerator = checked_negate(numerator);
      denominator = static_cast<I64>(-denominator);
    }
    if (numerator == 0) {
      denominator = 1;
      return;
    }
    const U64 common = std::gcd(magnitude(numerator), static_cast<U64>(denominator));
    numerator /= static_cast<I64>(common);
    denominator /= static_cast<I64>(common);
  }
};

[[nodiscard]] int compare_unsigned_ratio(U64 first_num, U64 first_den,
                                         U64 second_num, U64 second_den) {
  bool reversed = false;
  while (true) {
    const U64 first_quotient = first_num / first_den;
    const U64 second_quotient = second_num / second_den;
    if (first_quotient != second_quotient) {
      const int result = first_quotient < second_quotient ? -1 : 1;
      return reversed ? -result : result;
    }
    const U64 first_remainder = first_num % first_den;
    const U64 second_remainder = second_num % second_den;
    if (first_remainder == 0 || second_remainder == 0) {
      int result = 0;
      if (first_remainder == 0 && second_remainder != 0) {
        result = -1;
      } else if (first_remainder != 0 && second_remainder == 0) {
        result = 1;
      }
      return reversed ? -result : result;
    }
    first_num = first_den;
    first_den = first_remainder;
    second_num = second_den;
    second_den = second_remainder;
    reversed = !reversed;
  }
}

[[nodiscard]] int compare(const Fraction& first, const Fraction& second) {
  if (first.numerator < 0 && second.numerator >= 0) {
    return -1;
  }
  if (first.numerator >= 0 && second.numerator < 0) {
    return 1;
  }
  if (first.numerator >= 0) {
    return compare_unsigned_ratio(magnitude(first.numerator),
                                  static_cast<U64>(first.denominator),
                                  magnitude(second.numerator),
                                  static_cast<U64>(second.denominator));
  }
  return -compare_unsigned_ratio(magnitude(first.numerator),
                                 static_cast<U64>(first.denominator),
                                 magnitude(second.numerator),
                                 static_cast<U64>(second.denominator));
}

[[nodiscard]] bool less(const Fraction& first, const Fraction& second) {
  return compare(first, second) < 0;
}

[[nodiscard]] bool equal(const Fraction& first, const Fraction& second) {
  return compare(first, second) == 0;
}

[[nodiscard]] Fraction negate(const Fraction& value) {
  return Fraction(checked_negate(value.numerator), value.denominator);
}

[[nodiscard]] Fraction add(const Fraction& first, const Fraction& second) {
  const I64 common = std::gcd(first.denominator, second.denominator);
  const I64 first_scale = second.denominator / common;
  const I64 second_scale = first.denominator / common;
  const I64 left = checked_multiply(first.numerator, first_scale);
  const I64 right = checked_multiply(second.numerator, second_scale);
  const I64 denominator = checked_positive_multiply(first.denominator, first_scale);
  return Fraction(checked_add(left, right), denominator);
}

[[nodiscard]] Fraction subtract(const Fraction& first, const Fraction& second) {
  return add(first, negate(second));
}

[[nodiscard]] Fraction multiply(const Fraction& first, const Fraction& second) {
  if (first.numerator == 0 || second.numerator == 0) {
    return Fraction{};
  }
  const U64 first_cancel = std::gcd(magnitude(first.numerator),
                                    static_cast<U64>(second.denominator));
  const U64 second_cancel = std::gcd(magnitude(second.numerator),
                                     static_cast<U64>(first.denominator));
  const I64 first_num = first.numerator / static_cast<I64>(first_cancel);
  const I64 second_num = second.numerator / static_cast<I64>(second_cancel);
  const I64 first_den = first.denominator / static_cast<I64>(second_cancel);
  const I64 second_den = second.denominator / static_cast<I64>(first_cancel);
  return Fraction(checked_multiply(first_num, second_num),
                  checked_positive_multiply(first_den, second_den));
}

[[nodiscard]] Fraction divide(const Fraction& first, const Fraction& second) {
  if (second.numerator == 0) {
    throw std::domain_error("exact rational division by zero");
  }
  if (first.numerator == 0) {
    return Fraction{};
  }

  const U64 cancel_num = std::gcd(magnitude(first.numerator), magnitude(second.numerator));
  const I64 reduced_first_num = divide_signed_by_magnitude(first.numerator, cancel_num);
  const I64 reduced_second_num = divide_signed_by_magnitude(second.numerator, cancel_num);
  const I64 cancel_den = std::gcd(first.denominator, second.denominator);
  const I64 reduced_first_den = first.denominator / cancel_den;
  const I64 reduced_second_den = second.denominator / cancel_den;

  const bool negative = (reduced_first_num < 0) != (reduced_second_num < 0);
  const U64 denominator_mag = magnitude(reduced_second_num);
  if (denominator_mag > static_cast<U64>(std::numeric_limits<I64>::max())) {
    throw std::overflow_error("exact rational denominator overflow");
  }
  const I64 numerator = checked_multiply(
      signed_from_magnitude(magnitude(reduced_first_num), negative), reduced_second_den);
  const I64 denominator = checked_positive_multiply(
      reduced_first_den, static_cast<I64>(denominator_mag));
  return Fraction(numerator, denominator);
}

[[nodiscard]] Rational64 export_fraction(const Fraction& value) {
  return Rational64{value.numerator, value.denominator};
}

class Tableau {
 public:
  Tableau(const std::vector<std::vector<I64>>& coefficients,
          const std::vector<I64>& bounds, const std::vector<I64>& objective)
      : row_count_(bounds.size()), variable_count_(objective.size()),
        basic_(row_count_), nonbasic_(variable_count_ + 1),
        data_(row_count_ + 2,
              std::vector<Fraction>(variable_count_ + 2, Fraction{})) {
    for (std::size_t row = 0; row < row_count_; ++row) {
      for (std::size_t column = 0; column < variable_count_; ++column) {
        data_[row][column] = Fraction(coefficients[row][column]);
      }
      basic_[row] = static_cast<std::int64_t>(variable_count_ + row);
      data_[row][variable_count_] = Fraction(-1);
      data_[row][variable_count_ + 1] = Fraction(bounds[row]);
    }
    for (std::size_t column = 0; column < variable_count_; ++column) {
      nonbasic_[column] = static_cast<std::int64_t>(column);
      data_[row_count_][column] = Fraction(checked_negate(objective[column]));
    }
    nonbasic_[variable_count_] = -1;
    data_[row_count_ + 1][variable_count_] = Fraction(1);
  }

  [[nodiscard]] LinearProgramResult solve() {
    LinearProgramResult result;
    if (row_count_ != 0) {
      std::size_t row = 0;
      for (std::size_t candidate = 1; candidate < row_count_; ++candidate) {
        if (less(data_[candidate][variable_count_ + 1],
                 data_[row][variable_count_ + 1]) ||
            (equal(data_[candidate][variable_count_ + 1],
                   data_[row][variable_count_ + 1]) &&
             basic_[candidate] < basic_[row])) {
          row = candidate;
        }
      }
      if (less(data_[row][variable_count_ + 1], Fraction{})) {
        result.phase_one_used = true;
        pivot(row, variable_count_);
        if (!simplex(1, nullptr) ||
            less(data_[row_count_ + 1][variable_count_ + 1], Fraction{})) {
          result.status = LinearProgramStatus::Infeasible;
          result.pivot_count = pivot_count_;
          return result;
        }
        if (!equal(data_[row_count_ + 1][variable_count_ + 1], Fraction{})) {
          result.status = LinearProgramStatus::Infeasible;
          result.pivot_count = pivot_count_;
          return result;
        }
        for (std::size_t basic_row = 0; basic_row < row_count_; ++basic_row) {
          if (basic_[basic_row] == -1) {
            std::size_t column = choose_nonzero_column(basic_row);
            if (column < variable_count_ + 1) {
              pivot(basic_row, column);
            }
            break;
          }
        }
      }
    }

    std::size_t unbounded_column = variable_count_ + 1;
    if (!simplex(2, &unbounded_column)) {
      result.status = LinearProgramStatus::Unbounded;
      result.variables = current_solution();
      result.unbounded_direction = unbounded_ray(unbounded_column);
      result.objective = export_fraction(data_[row_count_][variable_count_ + 1]);
      result.pivot_count = pivot_count_;
      return result;
    }

    result.status = LinearProgramStatus::Optimal;
    result.variables = current_solution();
    result.objective = export_fraction(data_[row_count_][variable_count_ + 1]);
    result.pivot_count = pivot_count_;
    return result;
  }

 private:
  [[nodiscard]] std::size_t choose_nonzero_column(std::size_t row) const {
    std::size_t selected = variable_count_ + 1;
    for (std::size_t column = 0; column <= variable_count_; ++column) {
      if (nonbasic_[column] == -1 || equal(data_[row][column], Fraction{})) {
        continue;
      }
      if (selected == variable_count_ + 1 ||
          nonbasic_[column] < nonbasic_[selected]) {
        selected = column;
      }
    }
    return selected;
  }

  void pivot(std::size_t row, std::size_t column) {
    const Fraction inverse = divide(Fraction(1), data_[row][column]);
    for (std::size_t other_row = 0; other_row < row_count_ + 2; ++other_row) {
      if (other_row == row) {
        continue;
      }
      for (std::size_t other_column = 0; other_column < variable_count_ + 2;
           ++other_column) {
        if (other_column == column) {
          continue;
        }
        const Fraction term = multiply(
            multiply(data_[row][other_column], data_[other_row][column]), inverse);
        data_[other_row][other_column] =
            subtract(data_[other_row][other_column], term);
      }
    }
    for (std::size_t other_column = 0; other_column < variable_count_ + 2;
         ++other_column) {
      if (other_column != column) {
        data_[row][other_column] = multiply(data_[row][other_column], inverse);
      }
    }
    for (std::size_t other_row = 0; other_row < row_count_ + 2; ++other_row) {
      if (other_row != row) {
        data_[other_row][column] =
            negate(multiply(data_[other_row][column], inverse));
      }
    }
    data_[row][column] = inverse;
    std::swap(basic_[row], nonbasic_[column]);
    ++pivot_count_;
  }

  [[nodiscard]] bool simplex(int phase, std::size_t* unbounded_column) {
    const std::size_t objective_row = phase == 1 ? row_count_ + 1 : row_count_;
    while (true) {
      std::size_t column = variable_count_ + 1;
      for (std::size_t candidate = 0; candidate <= variable_count_; ++candidate) {
        if (phase == 2 && nonbasic_[candidate] == -1) {
          continue;
        }
        if (column == variable_count_ + 1 ||
            less(data_[objective_row][candidate], data_[objective_row][column]) ||
            (equal(data_[objective_row][candidate], data_[objective_row][column]) &&
             nonbasic_[candidate] < nonbasic_[column])) {
          column = candidate;
        }
      }
      if (column == variable_count_ + 1 ||
          !less(data_[objective_row][column], Fraction{})) {
        return true;
      }

      std::size_t row = row_count_;
      Fraction best_ratio;
      for (std::size_t candidate = 0; candidate < row_count_; ++candidate) {
        if (!less(Fraction{}, data_[candidate][column])) {
          continue;
        }
        const Fraction ratio = divide(data_[candidate][variable_count_ + 1],
                                      data_[candidate][column]);
        if (row == row_count_ || less(ratio, best_ratio) ||
            (equal(ratio, best_ratio) && basic_[candidate] < basic_[row])) {
          row = candidate;
          best_ratio = ratio;
        }
      }
      if (row == row_count_) {
        if (unbounded_column != nullptr) {
          *unbounded_column = column;
        }
        return false;
      }
      pivot(row, column);
    }
  }

  [[nodiscard]] std::vector<Rational64> current_solution() const {
    std::vector<Rational64> solution(variable_count_, Rational64{});
    for (std::size_t row = 0; row < row_count_; ++row) {
      if (basic_[row] >= 0 &&
          static_cast<std::size_t>(basic_[row]) < variable_count_) {
        solution[static_cast<std::size_t>(basic_[row])] =
            export_fraction(data_[row][variable_count_ + 1]);
      }
    }
    return solution;
  }

  [[nodiscard]] std::vector<Rational64> unbounded_ray(std::size_t column) const {
    std::vector<Fraction> direction(variable_count_, Fraction{});
    if (nonbasic_[column] >= 0 &&
        static_cast<std::size_t>(nonbasic_[column]) < variable_count_) {
      direction[static_cast<std::size_t>(nonbasic_[column])] = Fraction(1);
    }
    for (std::size_t row = 0; row < row_count_; ++row) {
      if (basic_[row] >= 0 &&
          static_cast<std::size_t>(basic_[row]) < variable_count_) {
        direction[static_cast<std::size_t>(basic_[row])] = negate(data_[row][column]);
      }
    }
    std::vector<Rational64> exported;
    exported.reserve(variable_count_);
    for (const Fraction& value : direction) {
      exported.push_back(export_fraction(value));
    }
    return exported;
  }

  std::size_t row_count_;
  std::size_t variable_count_;
  std::vector<std::int64_t> basic_;
  std::vector<std::int64_t> nonbasic_;
  std::vector<std::vector<Fraction>> data_;
  std::size_t pivot_count_ = 0;
};

}  // namespace

LinearProgramResult maximize_linear_program(
    const std::vector<std::vector<std::int64_t>>& coefficients,
    const std::vector<std::int64_t>& bounds,
    const std::vector<std::int64_t>& objective) {
  if (coefficients.size() != bounds.size()) {
    throw std::invalid_argument("linear-program row/bound size mismatch");
  }
  for (const auto& row : coefficients) {
    if (row.size() != objective.size()) {
      throw std::invalid_argument("linear-program coefficient width mismatch");
    }
  }
  if (objective.size() > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) ||
      bounds.size() > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) - objective.size()) {
    throw std::length_error("linear-program dimensions exceed index representation");
  }
  Tableau tableau(coefficients, bounds, objective);
  return tableau.solve();
}

}  // namespace algorithms::optimization
