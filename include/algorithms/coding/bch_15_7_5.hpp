#pragma once

#include "algorithms/polynomials/extension_field.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::coding {

inline constexpr std::size_t kBch1575MessageBits = 7U;
inline constexpr std::size_t kBch1575CodewordBits = 15U;
inline constexpr std::size_t kBch1575ParityBits = 8U;
inline constexpr std::size_t kBch1575MaxErrors = 2U;

using Bch1575Message =
    std::array<std::uint8_t, kBch1575MessageBits>;
using Bch1575Codeword =
    std::array<std::uint8_t, kBch1575CodewordBits>;

struct Bch1575DecodeResult {
  Bch1575Message message{};
  Bch1575Codeword corrected_codeword{};
  std::vector<std::size_t> error_positions;

  friend bool operator==(const Bch1575DecodeResult&,
                         const Bch1575DecodeResult&) = default;
};

namespace detail {

inline constexpr std::uint16_t kBch1575GeneratorMask =
    UINT16_C(0x01D1);  // x^8 + x^7 + x^6 + x^4 + 1.

[[nodiscard]] inline std::uint8_t element_to_nibble(
    const polynomials::PrimeFieldExtension::Element& element) {
  if (element.size() > 4U) {
    throw std::logic_error("BCH GF(16) element exceeds degree bound");
  }
  std::uint8_t value = 0U;
  for (std::size_t index = 0U; index < element.size(); ++index) {
    if (element[index] > 1U) {
      throw std::logic_error("BCH GF(16) element is not binary");
    }
    value |= static_cast<std::uint8_t>(element[index] << index);
  }
  return value;
}

[[nodiscard]] inline polynomials::PrimeFieldExtension::Element
nibble_to_element(const std::uint8_t value) {
  if (value > 15U) {
    throw std::logic_error("BCH GF(16) nibble out of range");
  }
  polynomials::PrimeFieldExtension::Element element(4U, 0U);
  for (std::size_t index = 0U; index < 4U; ++index) {
    element[index] = (value >> index) & 1U;
  }
  while (!element.empty() && element.back() == 0U) {
    element.pop_back();
  }
  return element;
}

struct Bch1575FieldTables {
  std::array<std::array<std::uint8_t, 16U>, 16U> multiply{};
  std::array<std::uint8_t, 16U> inverse{};
  std::array<std::uint8_t, 15U> alpha_power{};

  Bch1575FieldTables() {
    const polynomials::PrimeFieldExtension field(
        2U, {1U, 1U, 0U, 0U, 1U});  // x^4 + x + 1.

    for (std::uint8_t first = 0U; first < 16U; ++first) {
      for (std::uint8_t second = 0U; second < 16U; ++second) {
        multiply[first][second] = element_to_nibble(
            field.multiply(nibble_to_element(first),
                           nibble_to_element(second)));
      }
    }

    for (std::uint8_t value = 1U; value < 16U; ++value) {
      inverse[value] =
          element_to_nibble(field.inverse(nibble_to_element(value)));
    }

    alpha_power[0U] = 1U;
    for (std::size_t exponent = 1U;
         exponent < alpha_power.size(); ++exponent) {
      alpha_power[exponent] =
          multiply[alpha_power[exponent - 1U]][2U];
    }

    std::array<bool, 16U> seen{};
    for (const std::uint8_t value : alpha_power) {
      if (value == 0U || seen[value]) {
        throw std::logic_error(
            "x is not primitive for BCH GF(16) modulus");
      }
      seen[value] = true;
    }
    if (multiply[alpha_power.back()][2U] != 1U) {
      throw std::logic_error(
          "BCH GF(16) primitive element does not have order 15");
    }
  }

  [[nodiscard]] std::uint8_t divide(
      const std::uint8_t numerator,
      const std::uint8_t denominator) const {
    if (denominator == 0U) {
      throw std::logic_error("BCH GF(16) division by zero");
    }
    return multiply[numerator][inverse[denominator]];
  }
};

[[nodiscard]] inline const Bch1575FieldTables& bch1575_field() {
  static const Bch1575FieldTables tables;
  return tables;
}

inline void validate_binary_bits(
    const std::span<const std::uint8_t> bits,
    const std::size_t expected_size,
    const char* message) {
  if (bits.size() != expected_size) {
    throw std::invalid_argument(message);
  }
  for (const std::uint8_t bit : bits) {
    if (bit > 1U) {
      throw std::invalid_argument("BCH bits must be 0 or 1");
    }
  }
}

[[nodiscard]] inline std::uint16_t codeword_mask(
    const std::span<const std::uint8_t> bits) {
  std::uint16_t mask = 0U;
  for (std::size_t index = 0U; index < bits.size(); ++index) {
    if (bits[index] != 0U) {
      mask |= static_cast<std::uint16_t>(UINT16_C(1) << index);
    }
  }
  return mask;
}

[[nodiscard]] inline Bch1575Codeword codeword_from_mask(
    const std::uint16_t mask) {
  Bch1575Codeword result{};
  for (std::size_t index = 0U; index < result.size(); ++index) {
    result[index] =
        static_cast<std::uint8_t>((mask >> index) & UINT16_C(1));
  }
  return result;
}

[[nodiscard]] inline std::uint16_t polynomial_remainder(
    std::uint16_t value) noexcept {
  for (int degree = 14; degree >= 8; --degree) {
    if ((value &
         static_cast<std::uint16_t>(UINT16_C(1) << degree)) != 0U) {
      value ^= static_cast<std::uint16_t>(
          kBch1575GeneratorMask << (degree - 8));
    }
  }
  return static_cast<std::uint16_t>(value & UINT16_C(0x00FF));
}

[[nodiscard]] inline std::array<std::uint8_t, 4U> syndromes(
    const Bch1575Codeword& received) {
  const auto& field = bch1575_field();
  std::array<std::uint8_t, 4U> result{};

  for (std::size_t syndrome_index = 0U;
       syndrome_index < result.size(); ++syndrome_index) {
    const std::size_t root_exponent = syndrome_index + 1U;
    const std::uint8_t root =
        field.alpha_power[root_exponent % 15U];

    std::uint8_t accumulated = 0U;
    std::uint8_t power = 1U;
    for (const std::uint8_t bit : received) {
      if (bit != 0U) {
        accumulated ^= power;
      }
      power = field.multiply[power][root];
    }
    result[syndrome_index] = accumulated;
  }
  return result;
}

struct LocatorPolynomial {
  std::array<std::uint8_t, 5U> coefficients{};
  std::size_t degree = 0U;
};

[[nodiscard]] inline LocatorPolynomial berlekamp_massey(
    const std::array<std::uint8_t, 4U>& sequence) {
  const auto& field = bch1575_field();

  std::array<std::uint8_t, 5U> current{};
  std::array<std::uint8_t, 5U> previous{};
  current[0U] = 1U;
  previous[0U] = 1U;

  std::size_t length = 0U;
  std::size_t shift = 1U;
  std::uint8_t previous_discrepancy = 1U;

  for (std::size_t position = 0U;
       position < sequence.size(); ++position) {
    std::uint8_t discrepancy = sequence[position];
    for (std::size_t index = 1U; index <= length; ++index) {
      discrepancy ^= field.multiply[current[index]]
                                   [sequence[position - index]];
    }

    if (discrepancy == 0U) {
      ++shift;
      continue;
    }

    const auto saved = current;
    const std::uint8_t scale =
        field.divide(discrepancy, previous_discrepancy);

    for (std::size_t index = 0U;
         index + shift < current.size(); ++index) {
      if (previous[index] != 0U) {
        current[index + shift] ^=
            field.multiply[scale][previous[index]];
      }
    }

    if (2U * length <= position) {
      length = position + 1U - length;
      previous = saved;
      previous_discrepancy = discrepancy;
      shift = 1U;
    } else {
      ++shift;
    }
  }

  return LocatorPolynomial{current, length};
}

[[nodiscard]] inline std::uint8_t evaluate_locator(
    const LocatorPolynomial& locator,
    const std::uint8_t point) {
  const auto& field = bch1575_field();
  std::uint8_t value = 0U;
  for (std::size_t index = locator.degree + 1U;
       index-- > 0U;) {
    value = field.multiply[value][point] ^
            locator.coefficients[index];
  }
  return value;
}

[[nodiscard]] inline bool all_zero(
    const std::array<std::uint8_t, 4U>& values) noexcept {
  for (const std::uint8_t value : values) {
    if (value != 0U) {
      return false;
    }
  }
  return true;
}

}  // namespace detail

// Systematic primitive binary BCH(15,7,5) encoding.
//
// Bit vectors are polynomial coefficients in low-degree-first order.
// The seven message bits become codeword coefficients x^8..x^14; parity
// occupies x^0..x^7. The generator polynomial is
// g(x)=x^8+x^7+x^6+x^4+1.
[[nodiscard]] inline Bch1575Codeword bch_15_7_5_encode(
    const std::span<const std::uint8_t> message) {
  detail::validate_binary_bits(
      message, kBch1575MessageBits,
      "BCH(15,7,5) message must contain exactly 7 bits");

  std::uint16_t message_mask = 0U;
  for (std::size_t index = 0U; index < message.size(); ++index) {
    if (message[index] != 0U) {
      message_mask |=
          static_cast<std::uint16_t>(UINT16_C(1) << index);
    }
  }

  const std::uint16_t shifted =
      static_cast<std::uint16_t>(message_mask << kBch1575ParityBits);
  const std::uint16_t parity =
      detail::polynomial_remainder(shifted);
  return detail::codeword_from_mask(
      static_cast<std::uint16_t>(shifted ^ parity));
}

// Correct up to two bit errors in primitive binary BCH(15,7,5).
//
// Returns nullopt when the syndrome/locator/root checks do not produce a valid
// codeword within the designed radius. As with every bounded-distance decoder,
// corruption beyond two bits is not guaranteed to be detected: a received word
// may lie within distance two of a different codeword.
[[nodiscard]] inline std::optional<Bch1575DecodeResult>
bch_15_7_5_decode(
    const std::span<const std::uint8_t> received_bits) {
  detail::validate_binary_bits(
      received_bits, kBch1575CodewordBits,
      "BCH(15,7,5) received word must contain exactly 15 bits");

  Bch1575Codeword corrected{};
  std::copy(received_bits.begin(), received_bits.end(),
            corrected.begin());

  const auto initial_syndromes = detail::syndromes(corrected);
  std::vector<std::size_t> error_positions;

  if (!detail::all_zero(initial_syndromes)) {
    const detail::LocatorPolynomial locator =
        detail::berlekamp_massey(initial_syndromes);
    if (locator.degree == 0U ||
        locator.degree > kBch1575MaxErrors) {
      return std::nullopt;
    }

    const auto& field = detail::bch1575_field();
    for (std::size_t position = 0U;
         position < kBch1575CodewordBits; ++position) {
      const std::size_t inverse_exponent =
          (15U - (position % 15U)) % 15U;
      const std::uint8_t point =
          field.alpha_power[inverse_exponent];
      if (detail::evaluate_locator(locator, point) == 0U) {
        error_positions.push_back(position);
      }
    }

    if (error_positions.size() != locator.degree ||
        error_positions.size() > kBch1575MaxErrors) {
      return std::nullopt;
    }

    for (const std::size_t position : error_positions) {
      corrected[position] ^= 1U;
    }

    if (!detail::all_zero(detail::syndromes(corrected))) {
      return std::nullopt;
    }
  }

  Bch1575Message message{};
  for (std::size_t index = 0U; index < message.size(); ++index) {
    message[index] = corrected[kBch1575ParityBits + index];
  }

  if (bch_15_7_5_encode(message) != corrected) {
    return std::nullopt;
  }

  return Bch1575DecodeResult{
      message, corrected, std::move(error_positions)};
}

}  // namespace algorithms::coding
