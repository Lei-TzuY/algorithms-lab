#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::coding {

class HuffmanByteCodebook {
 public:
  explicit HuffmanByteCodebook(
      const std::array<std::uint64_t, 256>& frequencies);

  [[nodiscard]] std::size_t symbol_count() const noexcept;
  [[nodiscard]] std::uint64_t weighted_bit_count() const noexcept;
  [[nodiscard]] const std::array<std::uint64_t, 256>& frequencies() const noexcept;
  [[nodiscard]] std::optional<std::string_view> code(std::uint8_t symbol) const noexcept;

  [[nodiscard]] std::string encode_bits(std::span<const std::uint8_t> input) const;
  [[nodiscard]] std::vector<std::uint8_t> decode_bits(std::string_view bits) const;

  // Intentionally expensive diagnostic: replays leaf/code correspondence,
  // prefix-freeness, weighted cost, and tree reachability.
  [[nodiscard]] bool valid_codebook() const;

 private:
  struct Node {
    std::uint64_t weight{};
    std::uint16_t min_symbol{};
    std::optional<std::uint8_t> symbol;
    std::optional<std::size_t> left;
    std::optional<std::size_t> right;
  };

  std::array<std::uint64_t, 256> frequencies_{};
  std::array<std::string, 256> codes_{};
  std::vector<Node> nodes_;
  std::optional<std::size_t> root_;
  std::size_t symbol_count_{};
  std::uint64_t weighted_bit_count_{};
};

}  // namespace algorithms::coding
