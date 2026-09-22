#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::coding {

inline constexpr std::uint32_t kRansByteScaleBits = 12U;
inline constexpr std::uint32_t kRansByteTotalFrequency =
    std::uint32_t{1} << kRansByteScaleBits;
inline constexpr std::uint64_t kRansByteLowerBound =
    std::uint64_t{1} << 31U;

struct RansByteBlock {
  std::array<std::uint16_t, 256U> frequencies{};
  std::uint64_t state{kRansByteLowerBound};
  std::vector<std::uint8_t> bytes;
  std::size_t symbol_count{};

  friend bool operator==(const RansByteBlock&,
                         const RansByteBlock&) = default;
};

namespace rans_byte_detail {

[[nodiscard]] inline std::array<std::uint16_t, 256U>
normalize_frequencies(const std::string_view input) {
  std::array<std::uint16_t, 256U> frequencies{};
  if (input.empty()) {
    return frequencies;
  }

  if (input.size() >
      std::numeric_limits<std::size_t>::max() /
          static_cast<std::size_t>(kRansByteTotalFrequency)) {
    throw std::length_error(
        "rANS input is too large for deterministic normalization");
  }

  std::array<std::size_t, 256U> counts{};
  for (const char byte : input) {
    ++counts[static_cast<std::uint8_t>(
        static_cast<unsigned char>(byte))];
  }

  std::array<std::size_t, 256U> remainders{};
  std::vector<std::uint16_t> observed;
  observed.reserve(256U);

  std::size_t total = 0U;
  for (std::size_t symbol = 0U; symbol < counts.size(); ++symbol) {
    if (counts[symbol] == 0U) {
      continue;
    }

    const std::size_t scaled =
        counts[symbol] *
        static_cast<std::size_t>(kRansByteTotalFrequency);
    const std::size_t quotient = scaled / input.size();
    remainders[symbol] = scaled % input.size();

    const std::size_t normalized =
        std::max<std::size_t>(1U, quotient);
    frequencies[symbol] =
        static_cast<std::uint16_t>(normalized);
    total += normalized;
    observed.push_back(static_cast<std::uint16_t>(symbol));
  }

  if (total < kRansByteTotalFrequency) {
    std::sort(
        observed.begin(), observed.end(),
        [&](const std::uint16_t left, const std::uint16_t right) {
          if (remainders[left] != remainders[right]) {
            return remainders[left] > remainders[right];
          }
          return left < right;
        });

    std::size_t cursor = 0U;
    while (total < kRansByteTotalFrequency) {
      const std::uint16_t symbol =
          observed[cursor % observed.size()];
      ++frequencies[symbol];
      ++total;
      ++cursor;
    }
  } else {
    while (total > kRansByteTotalFrequency) {
      std::optional<std::uint16_t> donor;
      for (const std::uint16_t symbol : observed) {
        if (frequencies[symbol] <= 1U) {
          continue;
        }
        if (!donor.has_value() ||
            remainders[symbol] < remainders[*donor] ||
            (remainders[symbol] == remainders[*donor] &&
             frequencies[symbol] > frequencies[*donor]) ||
            (remainders[symbol] == remainders[*donor] &&
             frequencies[symbol] == frequencies[*donor] &&
             symbol < *donor)) {
          donor = symbol;
        }
      }
      if (!donor.has_value()) {
        throw std::logic_error(
            "rANS normalization cannot remove excess mass");
      }
      --frequencies[*donor];
      --total;
    }
  }

  if (total != kRansByteTotalFrequency) {
    throw std::logic_error(
        "rANS normalized frequencies do not sum to scale");
  }
  return frequencies;
}

[[nodiscard]] inline std::array<std::uint32_t, 256U>
cumulative_frequencies(
    const std::array<std::uint16_t, 256U>& frequencies) {
  std::array<std::uint32_t, 256U> cumulative{};
  std::uint32_t total = 0U;
  for (std::size_t symbol = 0U; symbol < frequencies.size(); ++symbol) {
    cumulative[symbol] = total;
    total += frequencies[symbol];
  }
  return cumulative;
}

}  // namespace rans_byte_detail

[[nodiscard]] inline RansByteBlock rans_encode_bytes(
    const std::string_view input) {
  RansByteBlock block;
  block.symbol_count = input.size();
  block.frequencies =
      rans_byte_detail::normalize_frequencies(input);

  if (input.empty()) {
    return block;
  }

  const auto cumulative =
      rans_byte_detail::cumulative_frequencies(block.frequencies);

  std::uint64_t state = kRansByteLowerBound;
  std::vector<std::uint8_t> emitted;
  emitted.reserve(input.size());

  for (std::size_t offset = input.size(); offset > 0U; --offset) {
    const std::uint8_t symbol = static_cast<std::uint8_t>(
        static_cast<unsigned char>(input[offset - 1U]));
    const std::uint64_t frequency = block.frequencies[symbol];
    const std::uint64_t start = cumulative[symbol];

    if (frequency == 0U) {
      throw std::logic_error(
          "rANS input symbol received zero normalized frequency");
    }

    const std::uint64_t state_limit =
        ((kRansByteLowerBound >> kRansByteScaleBits) << 8U) *
        frequency;
    while (state >= state_limit) {
      emitted.push_back(static_cast<std::uint8_t>(state & 0xFFU));
      state >>= 8U;
    }

    state =
        ((state / frequency) << kRansByteScaleBits) +
        (state % frequency) + start;
  }

  std::reverse(emitted.begin(), emitted.end());
  block.state = state;
  block.bytes = std::move(emitted);
  return block;
}

[[nodiscard]] inline std::string rans_decode_bytes(
    const RansByteBlock& block) {
  if (block.symbol_count == 0U) {
    const bool any_frequency = std::any_of(
        block.frequencies.begin(), block.frequencies.end(),
        [](const std::uint16_t frequency) {
          return frequency != 0U;
        });
    if (any_frequency || block.state != kRansByteLowerBound ||
        !block.bytes.empty()) {
      throw std::invalid_argument(
          "malformed empty rANS block");
    }
    return {};
  }

  std::uint32_t total = 0U;
  std::array<std::uint32_t, 256U> cumulative{};
  for (std::size_t symbol = 0U;
       symbol < block.frequencies.size(); ++symbol) {
    cumulative[symbol] = total;
    total += block.frequencies[symbol];
    if (total > kRansByteTotalFrequency) {
      throw std::invalid_argument(
          "rANS frequency model exceeds scale");
    }
  }
  if (total != kRansByteTotalFrequency) {
    throw std::invalid_argument(
        "rANS frequency model does not fill scale");
  }
  if (block.state < kRansByteLowerBound) {
    throw std::invalid_argument(
        "rANS initial state is below renormalization bound");
  }

  std::array<std::uint8_t, kRansByteTotalFrequency> lookup{};
  for (std::size_t symbol = 0U;
       symbol < block.frequencies.size(); ++symbol) {
    const std::uint32_t begin = cumulative[symbol];
    const std::uint32_t end =
        begin + block.frequencies[symbol];
    for (std::uint32_t slot = begin; slot < end; ++slot) {
      lookup[slot] = static_cast<std::uint8_t>(symbol);
    }
  }

  std::string output;
  if (block.symbol_count > output.max_size()) {
    throw std::length_error(
        "rANS decoded output exceeds string max_size");
  }
  output.reserve(block.symbol_count);

  std::uint64_t state = block.state;
  std::size_t byte_cursor = 0U;
  constexpr std::uint64_t kScaleMask =
      static_cast<std::uint64_t>(kRansByteTotalFrequency - 1U);

  for (std::size_t index = 0U;
       index < block.symbol_count; ++index) {
    const std::uint32_t residue =
        static_cast<std::uint32_t>(state & kScaleMask);
    const std::uint8_t symbol = lookup[residue];
    const std::uint64_t frequency = block.frequencies[symbol];
    const std::uint64_t offset =
        residue - cumulative[symbol];
    const std::uint64_t quotient =
        state >> kRansByteScaleBits;

    if (frequency == 0U ||
        quotient >
            (std::numeric_limits<std::uint64_t>::max() - offset) /
                frequency) {
      throw std::invalid_argument(
          "rANS decoder state transition overflow");
    }

    state = frequency * quotient + offset;
    while (state < kRansByteLowerBound) {
      if (byte_cursor >= block.bytes.size()) {
        throw std::invalid_argument(
            "truncated rANS renormalization stream");
      }
      state =
          (state << 8U) |
          static_cast<std::uint64_t>(block.bytes[byte_cursor++]);
    }

    output.push_back(static_cast<char>(symbol));
  }

  if (state != kRansByteLowerBound ||
      byte_cursor != block.bytes.size()) {
    throw std::invalid_argument(
        "rANS block has non-canonical terminal state or trailing bytes");
  }

  return output;
}

}  // namespace algorithms::coding
