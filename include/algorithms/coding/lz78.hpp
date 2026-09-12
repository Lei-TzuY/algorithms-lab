#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::coding {

struct Lz78Codeword {
  std::size_t prefix_index;
  std::optional<std::uint8_t> next_byte;

  friend bool operator==(const Lz78Codeword&, const Lz78Codeword&) = default;
};

// Greedy LZ78 parsing over arbitrary bytes. Dictionary index 0 is the empty
// phrase. If the input ends exactly at an already-known phrase, the final
// codeword has next_byte == nullopt and does not add a new dictionary entry.
[[nodiscard]] std::vector<Lz78Codeword> lz78_encode(std::string_view input);

// Decode a prefix-index LZ78 stream. Prefix indices must reference dictionary
// entries created by earlier non-terminal codewords. A terminal codeword is
// allowed only as the final codeword and must reference a non-empty phrase.
[[nodiscard]] std::string lz78_decode(std::span<const Lz78Codeword> codewords);

}  // namespace algorithms::coding
