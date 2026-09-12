#include "algorithms/coding/arithmetic_byte_code.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace algorithms::coding {
namespace {

constexpr std::uint32_t kTop = 0xffffffffU;
constexpr std::uint32_t kFirstQuarter = 0x40000000U;
constexpr std::uint32_t kHalf = 0x80000000U;
constexpr std::uint32_t kThirdQuarter = 0xc0000000U;

struct Model {
  std::array<std::uint64_t, 257> cumulative{};
  std::uint64_t total = 0;
};

Model build_model(const std::array<std::uint32_t, 256>& frequencies) {
  Model model;
  for (std::size_t symbol = 0; symbol < frequencies.size(); ++symbol) {
    model.cumulative[symbol + 1] =
        model.cumulative[symbol] + frequencies[symbol];
  }
  model.total = model.cumulative.back();
  if (model.total > kArithmeticMaxTotalFrequency) {
    throw std::invalid_argument("arithmetic model total exceeds supported bound");
  }
  return model;
}

void update_interval(std::uint32_t& low, std::uint32_t& high,
                     std::uint64_t cumulative_low,
                     std::uint64_t cumulative_high,
                     std::uint64_t total) {
  const std::uint64_t range =
      static_cast<std::uint64_t>(high) - low + 1ULL;
  const std::uint64_t base = low;
  const std::uint64_t next_high =
      base + (range * cumulative_high) / total - 1ULL;
  const std::uint64_t next_low =
      base + (range * cumulative_low) / total;
  if (next_low > next_high || next_high > kTop) {
    throw std::overflow_error("arithmetic interval became unrepresentable");
  }
  low = static_cast<std::uint32_t>(next_low);
  high = static_cast<std::uint32_t>(next_high);
}

void double_interval(std::uint32_t& low, std::uint32_t& high) {
  low = static_cast<std::uint32_t>(static_cast<std::uint64_t>(low) * 2ULL);
  high = static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(high) * 2ULL + 1ULL);
}

class BitReader {
 public:
  explicit BitReader(const std::vector<std::uint8_t>& bits) : bits_(bits) {
    for (const std::uint8_t bit : bits_) {
      if (bit > 1U) {
        throw std::invalid_argument("arithmetic stream contains non-binary bit");
      }
    }
  }

  std::uint32_t read() {
    if (position_ == bits_.size()) return 0U;
    return bits_[position_++];
  }

 private:
  const std::vector<std::uint8_t>& bits_;
  std::size_t position_ = 0;
};

}  // namespace

ArithmeticByteStream arithmetic_encode_bytes(
    std::string_view input,
    const std::array<std::uint32_t, 256>& frequencies) {
  const Model model = build_model(frequencies);
  ArithmeticByteStream result;
  result.symbol_count = input.size();
  if (input.empty()) return result;
  if (model.total == 0) {
    throw std::invalid_argument("non-empty arithmetic input requires a model");
  }

  std::uint32_t low = 0;
  std::uint32_t high = kTop;
  std::size_t pending_bits = 0;

  auto emit_with_pending = [&](std::uint8_t bit) {
    result.bits.push_back(bit);
    const std::uint8_t complement = static_cast<std::uint8_t>(1U - bit);
    result.bits.insert(result.bits.end(), pending_bits, complement);
    pending_bits = 0;
  };

  for (const char raw : input) {
    const auto symbol = static_cast<std::uint8_t>(
        static_cast<unsigned char>(raw));
    if (frequencies[symbol] == 0U) {
      throw std::invalid_argument("arithmetic input symbol has zero frequency");
    }

    update_interval(low, high, model.cumulative[symbol],
                    model.cumulative[static_cast<std::size_t>(symbol) + 1],
                    model.total);

    while (true) {
      if (high < kHalf) {
        emit_with_pending(0U);
      } else if (low >= kHalf) {
        emit_with_pending(1U);
        low -= kHalf;
        high -= kHalf;
      } else if (low >= kFirstQuarter && high < kThirdQuarter) {
        if (pending_bits == std::numeric_limits<std::size_t>::max()) {
          throw std::length_error("arithmetic pending-bit count overflow");
        }
        ++pending_bits;
        low -= kFirstQuarter;
        high -= kFirstQuarter;
      } else {
        break;
      }
      double_interval(low, high);
    }
  }

  if (pending_bits == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("arithmetic pending-bit count overflow");
  }
  ++pending_bits;
  if (low < kFirstQuarter) {
    emit_with_pending(0U);
  } else {
    emit_with_pending(1U);
  }
  return result;
}

std::string arithmetic_decode_bytes(
    const ArithmeticByteStream& stream,
    const std::array<std::uint32_t, 256>& frequencies) {
  const Model model = build_model(frequencies);
  BitReader reader(stream.bits);
  if (stream.symbol_count == 0) {
    if (!stream.bits.empty()) {
      throw std::invalid_argument("empty arithmetic stream must not contain bits");
    }
    return {};
  }
  if (model.total == 0) {
    throw std::invalid_argument("non-empty arithmetic stream requires a model");
  }
  if (stream.bits.empty()) {
    throw std::invalid_argument("non-empty arithmetic stream requires bits");
  }

  std::uint32_t low = 0;
  std::uint32_t high = kTop;
  std::uint32_t value = 0;
  for (int bit = 0; bit < 32; ++bit) {
    value = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(value) * 2ULL + reader.read());
  }

  std::string output;
  output.reserve(stream.symbol_count);

  for (std::size_t index = 0; index < stream.symbol_count; ++index) {
    const std::uint64_t range =
        static_cast<std::uint64_t>(high) - low + 1ULL;
    const std::uint64_t offset =
        static_cast<std::uint64_t>(value) - low + 1ULL;
    const std::uint64_t scaled =
        (offset * model.total - 1ULL) / range;

    const auto upper = std::upper_bound(model.cumulative.begin(),
                                        model.cumulative.end(), scaled);
    if (upper == model.cumulative.begin() || upper == model.cumulative.end()) {
      throw std::invalid_argument("arithmetic stream falls outside model");
    }
    const std::size_t symbol =
        static_cast<std::size_t>(upper - model.cumulative.begin() - 1);
    if (symbol >= frequencies.size() || frequencies[symbol] == 0U) {
      throw std::invalid_argument("arithmetic stream selected zero-frequency symbol");
    }

    update_interval(low, high, model.cumulative[symbol],
                    model.cumulative[symbol + 1], model.total);
    output.push_back(static_cast<char>(static_cast<unsigned char>(symbol)));

    while (true) {
      if (high < kHalf) {
        // no offset adjustment
      } else if (low >= kHalf) {
        value -= kHalf;
        low -= kHalf;
        high -= kHalf;
      } else if (low >= kFirstQuarter && high < kThirdQuarter) {
        value -= kFirstQuarter;
        low -= kFirstQuarter;
        high -= kFirstQuarter;
      } else {
        break;
      }
      double_interval(low, high);
      value = static_cast<std::uint32_t>(
          static_cast<std::uint64_t>(value) * 2ULL + reader.read());
    }
  }

  return output;
}

}  // namespace algorithms::coding
