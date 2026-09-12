#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::coding {

struct Lz77Token {
  std::size_t distance;
  std::size_t length;
  std::optional<std::uint8_t> literal;

  friend bool operator==(const Lz77Token&, const Lz77Token&) = default;
};

namespace detail {

constexpr std::size_t kLz77MinimumMatch = 3;

inline std::uint32_t lz77_key_at(std::string_view input, std::size_t position) {
  const auto a = static_cast<std::uint32_t>(
      static_cast<unsigned char>(input[position]));
  const auto b = static_cast<std::uint32_t>(
      static_cast<unsigned char>(input[position + 1]));
  const auto c = static_cast<std::uint32_t>(
      static_cast<unsigned char>(input[position + 2]));
  return (a << 16U) | (b << 8U) | c;
}

}  // namespace detail

// Greedy LZ77 parsing over arbitrary bytes. A literal token has distance=0,
// length=0, and literal set. A match token has literal=nullopt, distance>0,
// and length>=3. Match copying may overlap the bytes being produced.
// Among equally long matches, the nearest source (smallest distance) wins.
[[nodiscard]] inline std::vector<Lz77Token> lz77_encode(
    std::string_view input, std::size_t window_size = 4096,
    std::size_t max_match_length = 258) {
  if (window_size == 0) {
    throw std::invalid_argument("LZ77 window size must be positive");
  }
  if (max_match_length < detail::kLz77MinimumMatch) {
    throw std::invalid_argument("LZ77 maximum match length must be at least 3");
  }

  std::map<std::uint32_t, std::vector<std::size_t>> triplet_positions;
  if (input.size() >= detail::kLz77MinimumMatch) {
    for (std::size_t position = 0;
         position + detail::kLz77MinimumMatch <= input.size(); ++position) {
      triplet_positions[detail::lz77_key_at(input, position)].push_back(position);
    }
  }

  std::vector<Lz77Token> tokens;
  std::size_t position = 0;
  while (position < input.size()) {
    std::size_t best_distance = 0;
    std::size_t best_length = 0;

    if (position + detail::kLz77MinimumMatch <= input.size()) {
      const auto found = triplet_positions.find(detail::lz77_key_at(input, position));
      if (found != triplet_positions.end()) {
        const auto& candidates = found->second;
        const std::size_t window_begin =
            position > window_size ? position - window_size : 0;
        auto begin = std::lower_bound(candidates.begin(), candidates.end(), window_begin);
        auto end = std::lower_bound(candidates.begin(), candidates.end(), position);
        const std::size_t limit = std::min(max_match_length, input.size() - position);

        while (end != begin) {
          --end;
          const std::size_t candidate = *end;
          const std::size_t distance = position - candidate;
          std::size_t length = detail::kLz77MinimumMatch;
          while (length < limit &&
                 input[position + length] == input[candidate + length]) {
            ++length;
          }
          if (length > best_length) {
            best_length = length;
            best_distance = distance;
            if (best_length == limit) break;
          }
        }
      }
    }

    if (best_length >= detail::kLz77MinimumMatch) {
      tokens.push_back(Lz77Token{best_distance, best_length, std::nullopt});
      position += best_length;
    } else {
      const auto byte = static_cast<std::uint8_t>(
          static_cast<unsigned char>(input[position]));
      tokens.push_back(Lz77Token{0, 0, byte});
      ++position;
    }
  }
  return tokens;
}

// Decode canonical literal/match tokens. Match sources must already begin in
// the decoded prefix; byte-wise copying intentionally permits overlap.
[[nodiscard]] inline std::string lz77_decode(std::span<const Lz77Token> tokens) {
  std::string output;
  for (const Lz77Token& token : tokens) {
    if (token.literal.has_value()) {
      if (token.distance != 0 || token.length != 0) {
        throw std::invalid_argument("LZ77 literal token has match fields");
      }
      output.push_back(static_cast<char>(*token.literal));
      continue;
    }

    if (token.distance == 0 || token.length < detail::kLz77MinimumMatch) {
      throw std::invalid_argument("LZ77 match token is malformed");
    }
    if (token.distance > output.size()) {
      throw std::invalid_argument("LZ77 match source precedes decoded prefix");
    }
    if (token.length > output.max_size() - output.size()) {
      throw std::length_error("LZ77 decoded output length is unrepresentable");
    }
    for (std::size_t copied = 0; copied < token.length; ++copied) {
      output.push_back(output[output.size() - token.distance]);
    }
  }
  return output;
}

}  // namespace algorithms::coding
