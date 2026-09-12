#include "algorithms/coding/arithmetic_byte_code.hpp"
#include "test_framework.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using algorithms::coding::ArithmeticByteStream;
using algorithms::coding::arithmetic_decode_bytes;
using algorithms::coding::arithmetic_encode_bytes;
using algorithms::coding::kArithmeticMaxTotalFrequency;

std::array<std::uint32_t, 256> uniform_model() {
  std::array<std::uint32_t, 256> frequencies{};
  frequencies.fill(1U);
  return frequencies;
}

struct RationalInterval {
  std::uint64_t low_num = 0;
  std::uint64_t high_num = 1;
  std::uint64_t denominator = 1;
};

RationalInterval exact_binary_source_interval(
    const std::vector<int>& sequence, std::uint64_t first_frequency,
    std::uint64_t second_frequency) {
  RationalInterval interval;
  const std::uint64_t total = first_frequency + second_frequency;
  for (const int symbol : sequence) {
    const std::uint64_t range_num = interval.high_num - interval.low_num;
    const std::uint64_t old_denominator = interval.denominator;
    const std::uint64_t base = interval.low_num * total;
    if (symbol == 0) {
      interval.low_num = base;
      interval.high_num = base + range_num * first_frequency;
    } else {
      interval.low_num = base + range_num * first_frequency;
      interval.high_num = base + range_num * total;
    }
    interval.denominator = old_denominator * total;
  }
  return interval;
}

std::uint64_t prefix_numerator(const std::vector<std::uint8_t>& bits) {
  std::uint64_t value = 0;
  for (const std::uint8_t bit : bits) value = value * 2U + bit;
  return value;
}

}  // namespace

TEST_CASE(arithmetic_byte_code_known_bits_and_arbitrary_bytes) {
  std::array<std::uint32_t, 256> binary{};
  binary[static_cast<unsigned char>('A')] = 1U;
  binary[static_cast<unsigned char>('B')] = 1U;
  const ArithmeticByteStream stream = arithmetic_encode_bytes("ABBA", binary);
  const std::vector<std::uint8_t> expected{0U, 1U, 1U, 0U, 0U, 1U};
  REQUIRE_EQ(stream.bits, expected);
  REQUIRE_EQ(stream.symbol_count, 4U);
  REQUIRE_EQ(arithmetic_decode_bytes(stream, binary), std::string("ABBA"));

  std::string bytes;
  bytes.push_back('\0');
  bytes.push_back(static_cast<char>(0x80));
  bytes.push_back(static_cast<char>(0xff));
  bytes.push_back('x');
  const auto model = uniform_model();
  const ArithmeticByteStream encoded = arithmetic_encode_bytes(bytes, model);
  REQUIRE_EQ(arithmetic_decode_bytes(encoded, model), bytes);
  REQUIRE(arithmetic_encode_bytes(bytes, model) == encoded);
}

TEST_CASE(arithmetic_byte_code_validates_model_and_stream) {
  std::array<std::uint32_t, 256> zero{};
  REQUIRE(arithmetic_encode_bytes("", zero).bits.empty());
  REQUIRE(arithmetic_decode_bytes(ArithmeticByteStream{}, zero).empty());
  REQUIRE_THROWS_AS(arithmetic_encode_bytes("x", zero), std::invalid_argument);

  auto sparse = zero;
  sparse[static_cast<unsigned char>('a')] = 1U;
  REQUIRE_THROWS_AS(arithmetic_encode_bytes("b", sparse), std::invalid_argument);

  auto too_big = zero;
  too_big[0] = kArithmeticMaxTotalFrequency;
  too_big[1] = 1U;
  REQUIRE_THROWS_AS(arithmetic_encode_bytes("", too_big), std::invalid_argument);

  const ArithmeticByteStream bad_bits{{0U, 2U, 1U}, 1U};
  REQUIRE_THROWS_AS(arithmetic_decode_bytes(bad_bits, uniform_model()),
                    std::invalid_argument);

  const ArithmeticByteStream bad_empty{{0U}, 0U};
  REQUIRE_THROWS_AS(arithmetic_decode_bytes(bad_empty, uniform_model()),
                    std::invalid_argument);
}

TEST_CASE(arithmetic_byte_code_emitted_prefix_lies_in_exact_source_interval) {
  std::array<std::uint32_t, 256> model{};
  model[static_cast<unsigned char>('a')] = 1U;
  model[static_cast<unsigned char>('b')] = 2U;
  std::mt19937_64 rng(0xA11CEULL);

  for (int trial = 0; trial < 1000; ++trial) {
    const int length = static_cast<int>(rng() % 6U) + 1;
    std::string text;
    std::vector<int> sequence;
    for (int index = 0; index < length; ++index) {
      const int symbol = static_cast<int>(rng() & 1U);
      sequence.push_back(symbol);
      text.push_back(symbol == 0 ? 'a' : 'b');
    }

    const ArithmeticByteStream stream = arithmetic_encode_bytes(text, model);
    REQUIRE(stream.bits.size() < 32U);
    const RationalInterval source =
        exact_binary_source_interval(sequence, 1U, 2U);
    const std::uint64_t code = prefix_numerator(stream.bits);
    const std::uint64_t dyadic_denominator = 1ULL << stream.bits.size();

    REQUIRE(code * source.denominator >=
            source.low_num * dyadic_denominator);
    REQUIRE((code + 1U) * source.denominator <=
            source.high_num * dyadic_denominator);
    REQUIRE_EQ(arithmetic_decode_bytes(stream, model), text);
  }
}

TEST_CASE(arithmetic_byte_code_randomized_arbitrary_byte_roundtrip) {
  const auto model = uniform_model();
  std::mt19937_64 rng(0xC0DEC0DEULL);
  for (int trial = 0; trial < 2500; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 129U);
    std::string text(length, '\0');
    for (char& byte : text) {
      byte = static_cast<char>(static_cast<unsigned char>(rng() & 0xffU));
    }
    const ArithmeticByteStream stream = arithmetic_encode_bytes(text, model);
    REQUIRE_EQ(stream.symbol_count, text.size());
    REQUIRE_EQ(arithmetic_decode_bytes(stream, model), text);
  }
}

TEST_CASE(arithmetic_byte_code_randomized_sparse_models_and_frequency_bound) {
  std::mt19937_64 rng(0x51A7C0DEULL);
  for (int trial = 0; trial < 1500; ++trial) {
    std::array<std::uint32_t, 256> model{};
    std::vector<std::uint8_t> active;
    const std::size_t active_count =
        1U + static_cast<std::size_t>(rng() % 24U);
    while (active.size() < active_count) {
      const auto symbol = static_cast<std::uint8_t>(rng() & 0xffU);
      if (model[symbol] != 0U) continue;
      model[symbol] = 1U + static_cast<std::uint32_t>(rng() % 1000U);
      active.push_back(symbol);
    }

    const std::size_t length = static_cast<std::size_t>(rng() % 160U);
    std::string text(length, '\0');
    for (char& byte : text) {
      const std::uint8_t symbol =
          active[static_cast<std::size_t>(rng() % active.size())];
      byte = static_cast<char>(symbol);
    }

    const ArithmeticByteStream stream = arithmetic_encode_bytes(text, model);
    REQUIRE_EQ(arithmetic_decode_bytes(stream, model), text);
    REQUIRE(arithmetic_encode_bytes(text, model) == stream);
  }

  std::array<std::uint32_t, 256> maximal{};
  maximal[0xffU] = kArithmeticMaxTotalFrequency;
  const std::string repeated(64, static_cast<char>(0xff));
  const ArithmeticByteStream stream = arithmetic_encode_bytes(repeated, maximal);
  REQUIRE_EQ(arithmetic_decode_bytes(stream, maximal), repeated);
}
