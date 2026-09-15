#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::number_theory {

struct KaratsubaDecimalProduct {
  std::string product;
  std::size_t recursive_calls{};
  std::size_t karatsuba_nodes{};
  std::size_t schoolbook_limb_products{};
};

namespace karatsuba_detail {

constexpr std::uint32_t kBase = 1'000'000'000U;
constexpr std::size_t kDecimalDigitsPerLimb = 9;
constexpr std::size_t kSchoolbookCutoff = 16;

using Limbs = std::vector<std::uint32_t>;

inline void trim(Limbs& value) {
  while (!value.empty() && value.back() == 0U) {
    value.pop_back();
  }
}

inline void checked_add_counter(std::size_t& target, std::size_t addend) {
  if (addend > std::numeric_limits<std::size_t>::max() - target) {
    throw std::overflow_error("karatsuba diagnostic counter overflow");
  }
  target += addend;
}

inline std::size_t checked_product(std::size_t first, std::size_t second) {
  if (first != 0U && second > std::numeric_limits<std::size_t>::max() / first) {
    throw std::overflow_error("karatsuba diagnostic counter overflow");
  }
  return first * second;
}

inline Limbs add(const Limbs& first, const Limbs& second) {
  const std::size_t count = std::max(first.size(), second.size());
  Limbs result;
  result.reserve(count + 1U);
  std::uint64_t carry = 0U;
  for (std::size_t index = 0; index < count; ++index) {
    const std::uint64_t left = index < first.size() ? first[index] : 0U;
    const std::uint64_t right = index < second.size() ? second[index] : 0U;
    const std::uint64_t sum = left + right + carry;
    result.push_back(static_cast<std::uint32_t>(sum % kBase));
    carry = sum / kBase;
  }
  if (carry != 0U) {
    result.push_back(static_cast<std::uint32_t>(carry));
  }
  return result;
}

inline int compare(const Limbs& first, const Limbs& second) {
  if (first.size() != second.size()) {
    return first.size() < second.size() ? -1 : 1;
  }
  for (std::size_t index = first.size(); index > 0; --index) {
    if (first[index - 1U] != second[index - 1U]) {
      return first[index - 1U] < second[index - 1U] ? -1 : 1;
    }
  }
  return 0;
}

inline Limbs subtract(const Limbs& first, const Limbs& second) {
  if (compare(first, second) < 0) {
    throw std::logic_error("karatsuba internal subtraction became negative");
  }
  Limbs result(first.size(), 0U);
  std::int64_t borrow = 0;
  for (std::size_t index = 0; index < first.size(); ++index) {
    const std::int64_t left = static_cast<std::int64_t>(first[index]);
    const std::int64_t right =
        index < second.size() ? static_cast<std::int64_t>(second[index]) : 0;
    std::int64_t current = left - right - borrow;
    if (current < 0) {
      current += static_cast<std::int64_t>(kBase);
      borrow = 1;
    } else {
      borrow = 0;
    }
    result[index] = static_cast<std::uint32_t>(current);
  }
  if (borrow != 0) {
    throw std::logic_error("karatsuba internal subtraction borrow remained");
  }
  trim(result);
  return result;
}

inline Limbs shifted(const Limbs& value, std::size_t limb_shift) {
  if (value.empty()) {
    return {};
  }
  if (limb_shift > std::numeric_limits<std::size_t>::max() - value.size()) {
    throw std::length_error("karatsuba shifted result is too large");
  }
  Limbs result(limb_shift, 0U);
  result.insert(result.end(), value.begin(), value.end());
  return result;
}

inline Limbs schoolbook(const Limbs& first, const Limbs& second,
                        KaratsubaDecimalProduct& diagnostics) {
  if (first.empty() || second.empty()) {
    return {};
  }
  checked_add_counter(diagnostics.schoolbook_limb_products,
                      checked_product(first.size(), second.size()));
  if (first.size() > std::numeric_limits<std::size_t>::max() - second.size()) {
    throw std::length_error("karatsuba schoolbook result is too large");
  }
  Limbs result(first.size() + second.size(), 0U);
  for (std::size_t left_index = 0; left_index < first.size(); ++left_index) {
    std::uint64_t carry = 0U;
    for (std::size_t right_index = 0; right_index < second.size(); ++right_index) {
      const std::size_t output_index = left_index + right_index;
      const std::uint64_t current =
          static_cast<std::uint64_t>(result[output_index]) +
          static_cast<std::uint64_t>(first[left_index]) *
              static_cast<std::uint64_t>(second[right_index]) +
          carry;
      result[output_index] = static_cast<std::uint32_t>(current % kBase);
      carry = current / kBase;
    }
    std::size_t output_index = left_index + second.size();
    while (carry != 0U) {
      if (output_index >= result.size()) {
        result.push_back(0U);
      }
      const std::uint64_t current =
          static_cast<std::uint64_t>(result[output_index]) + carry;
      result[output_index] = static_cast<std::uint32_t>(current % kBase);
      carry = current / kBase;
      ++output_index;
    }
  }
  trim(result);
  return result;
}

inline Limbs slice(const Limbs& value, std::size_t begin, std::size_t end) {
  if (begin >= value.size() || begin >= end) {
    return {};
  }
  end = std::min(end, value.size());
  Limbs result(value.begin() + static_cast<std::ptrdiff_t>(begin),
               value.begin() + static_cast<std::ptrdiff_t>(end));
  trim(result);
  return result;
}

inline Limbs multiply(const Limbs& first, const Limbs& second,
                      KaratsubaDecimalProduct& diagnostics) {
  checked_add_counter(diagnostics.recursive_calls, 1U);
  if (first.empty() || second.empty()) {
    return {};
  }
  const std::size_t smaller = std::min(first.size(), second.size());
  const std::size_t larger = std::max(first.size(), second.size());
  if (smaller <= kSchoolbookCutoff || larger - smaller > smaller) {
    return schoolbook(first, second, diagnostics);
  }

  checked_add_counter(diagnostics.karatsuba_nodes, 1U);
  const std::size_t split = larger / 2U;
  const Limbs first_low = slice(first, 0U, split);
  const Limbs first_high = slice(first, split, first.size());
  const Limbs second_low = slice(second, 0U, split);
  const Limbs second_high = slice(second, split, second.size());

  const Limbs low_product = multiply(first_low, second_low, diagnostics);
  const Limbs high_product = multiply(first_high, second_high, diagnostics);
  const Limbs first_sum = add(first_low, first_high);
  const Limbs second_sum = add(second_low, second_high);
  const Limbs sum_product = multiply(first_sum, second_sum, diagnostics);

  const Limbs without_low = subtract(sum_product, low_product);
  const Limbs cross = subtract(without_low, high_product);
  Limbs result = add(low_product, shifted(cross, split));
  result = add(result, shifted(high_product, split * 2U));
  trim(result);
  return result;
}

inline Limbs parse_decimal(std::string_view text) {
  if (text.empty()) {
    throw std::invalid_argument("karatsuba operands must be non-empty decimal strings");
  }
  for (const char ch : text) {
    if (ch < '0' || ch > '9') {
      throw std::invalid_argument("karatsuba operands must contain only decimal digits");
    }
  }

  Limbs result;
  const std::size_t group_count =
      text.size() / kDecimalDigitsPerLimb +
      (text.size() % kDecimalDigitsPerLimb == 0U ? 0U : 1U);
  result.reserve(group_count);
  std::size_t end = text.size();
  while (end != 0U) {
    const std::size_t begin =
        end > kDecimalDigitsPerLimb ? end - kDecimalDigitsPerLimb : 0U;
    std::uint32_t limb = 0U;
    for (std::size_t index = begin; index < end; ++index) {
      limb = static_cast<std::uint32_t>(limb * 10U +
                                        static_cast<unsigned>(text[index] - '0'));
    }
    result.push_back(limb);
    end = begin;
  }
  trim(result);
  return result;
}

inline std::string format_decimal(const Limbs& value) {
  if (value.empty()) {
    return "0";
  }
  std::string output = std::to_string(value.back());
  for (std::size_t index = value.size() - 1U; index > 0; --index) {
    const std::string group = std::to_string(value[index - 1U]);
    output.append(kDecimalDigitsPerLimb - group.size(), '0');
    output += group;
  }
  return output;
}

}  // namespace karatsuba_detail

inline KaratsubaDecimalProduct karatsuba_multiply_decimal(
    std::string_view first, std::string_view second) {
  KaratsubaDecimalProduct result;
  const karatsuba_detail::Limbs left = karatsuba_detail::parse_decimal(first);
  const karatsuba_detail::Limbs right = karatsuba_detail::parse_decimal(second);
  const karatsuba_detail::Limbs product =
      karatsuba_detail::multiply(left, right, result);
  result.product = karatsuba_detail::format_decimal(product);
  return result;
}

}  // namespace algorithms::number_theory
