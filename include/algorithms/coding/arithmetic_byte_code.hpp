#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::coding {

inline constexpr std::uint32_t kArithmeticMaxTotalFrequency = 1U << 20U;

struct ArithmeticByteStream {
  std::vector<std::uint8_t> bits;
  std::size_t symbol_count = 0;

  friend bool operator==(const ArithmeticByteStream&,
                         const ArithmeticByteStream&) = default;
};

// Static-model arithmetic coding over arbitrary bytes. The frequency model is
// shared out-of-band between encoder and decoder. Frequencies may be zero, but
// the total must not exceed kArithmeticMaxTotalFrequency. Every encoded input
// symbol must have positive model frequency.
[[nodiscard]] ArithmeticByteStream arithmetic_encode_bytes(
    std::string_view input,
    const std::array<std::uint32_t, 256>& frequencies);

// Decode exactly stream.symbol_count bytes using the same static model. Bits
// are logical binary digits stored as bytes 0/1; missing tail bits are the
// standard zero-extension used by finite-precision arithmetic decoding.
[[nodiscard]] std::string arithmetic_decode_bytes(
    const ArithmeticByteStream& stream,
    const std::array<std::uint32_t, 256>& frequencies);

}  // namespace algorithms::coding
