#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::coding {

using LzwCode = std::uint32_t;

namespace lzw_detail {

constexpr LzwCode kAlphabetSize = 256U;

[[nodiscard]] inline std::uint8_t to_byte(const char value) noexcept {
  return static_cast<std::uint8_t>(static_cast<unsigned char>(value));
}

[[nodiscard]] inline char to_char(const std::uint8_t value) noexcept {
  return static_cast<char>(value);
}

struct DecoderEntry {
  LzwCode prefix{};
  std::uint8_t suffix{};
};

[[nodiscard]] inline std::string decode_existing_code(
    const LzwCode code, const std::vector<DecoderEntry>& dictionary) {
  std::string reversed;
  LzwCode cursor = code;
  while (cursor >= kAlphabetSize) {
    const std::size_t index =
        static_cast<std::size_t>(cursor - kAlphabetSize);
    if (index >= dictionary.size()) {
      throw std::invalid_argument("LZW code references an unknown dictionary entry");
    }
    const DecoderEntry& entry = dictionary[index];
    if (entry.prefix >= cursor) {
      throw std::invalid_argument("LZW dictionary prefix is not backward-referencing");
    }
    reversed.push_back(to_char(entry.suffix));
    cursor = entry.prefix;
  }
  reversed.push_back(to_char(static_cast<std::uint8_t>(cursor)));
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

inline void append_checked(std::string& output, const std::string_view phrase) {
  if (phrase.size() > output.max_size() - output.size()) {
    throw std::length_error("LZW decoded output is too large");
  }
  output.append(phrase.data(), phrase.size());
}

}  // namespace lzw_detail

// Classical byte-oriented LZW with a fixed initial 256-symbol dictionary.
// The dictionary grows monotonically until the uint32_t code space is exhausted,
// at which point both encoder and decoder freeze it. Codes are returned directly;
// bit packing, adaptive code widths, dictionary reset policies, and file framing
// are intentionally outside this recovery slice.
[[nodiscard]] inline std::vector<LzwCode> lzw_encode_bytes(
    const std::string_view input) {
  using lzw_detail::kAlphabetSize;
  if (input.empty()) {
    return {};
  }

  std::map<std::pair<LzwCode, std::uint8_t>, LzwCode> transitions;
  std::vector<LzwCode> output;
  output.reserve(input.size());

  std::uint64_t next_code = kAlphabetSize;
  LzwCode current = static_cast<LzwCode>(lzw_detail::to_byte(input.front()));

  for (std::size_t index = 1U; index < input.size(); ++index) {
    const std::uint8_t byte = lzw_detail::to_byte(input[index]);
    const auto key = std::make_pair(current, byte);
    const auto found = transitions.find(key);
    if (found != transitions.end()) {
      current = found->second;
      continue;
    }

    output.push_back(current);
    if (next_code <= std::numeric_limits<LzwCode>::max()) {
      const LzwCode assigned = static_cast<LzwCode>(next_code);
      transitions.emplace(key, assigned);
      ++next_code;
    }
    current = static_cast<LzwCode>(byte);
  }

  output.push_back(current);
  return output;
}

[[nodiscard]] inline std::string lzw_decode_bytes(
    const std::vector<LzwCode>& codes) {
  using lzw_detail::DecoderEntry;
  using lzw_detail::kAlphabetSize;

  if (codes.empty()) {
    return {};
  }
  if (codes.front() >= kAlphabetSize) {
    throw std::invalid_argument("LZW first code must be a literal byte");
  }

  std::vector<DecoderEntry> dictionary;
  dictionary.reserve(codes.size());

  LzwCode previous_code = codes.front();
  std::string previous_phrase =
      lzw_detail::decode_existing_code(previous_code, dictionary);
  std::string output;
  output.reserve(codes.size());
  lzw_detail::append_checked(output, previous_phrase);

  for (std::size_t index = 1U; index < codes.size(); ++index) {
    const LzwCode code = codes[index];
    const std::uint64_t next_code =
        static_cast<std::uint64_t>(kAlphabetSize) + dictionary.size();

    std::string phrase;
    if (static_cast<std::uint64_t>(code) < next_code) {
      phrase = lzw_detail::decode_existing_code(code, dictionary);
    } else if (static_cast<std::uint64_t>(code) == next_code &&
               next_code <= std::numeric_limits<LzwCode>::max()) {
      phrase = previous_phrase;
      phrase.push_back(previous_phrase.front());
    } else {
      throw std::invalid_argument("LZW code skips ahead of dictionary growth");
    }

    lzw_detail::append_checked(output, phrase);

    if (next_code <= std::numeric_limits<LzwCode>::max()) {
      dictionary.push_back(
          DecoderEntry{previous_code, lzw_detail::to_byte(phrase.front())});
    }

    previous_code = code;
    previous_phrase = std::move(phrase);
  }

  return output;
}

}  // namespace algorithms::coding
