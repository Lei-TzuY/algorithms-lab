#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct AhoCorasickMatch {
  std::size_t pattern_index;
  std::size_t begin;
  std::size_t end;

  friend bool operator==(const AhoCorasickMatch&, const AhoCorasickMatch&) = default;
};

class AhoCorasickByteMatcher {
 public:
  explicit AhoCorasickByteMatcher(const std::vector<std::string>& patterns);

  [[nodiscard]] std::size_t pattern_count() const noexcept;
  [[nodiscard]] std::size_t state_count() const noexcept;
  [[nodiscard]] std::vector<AhoCorasickMatch> find_all(std::string_view text) const;

 private:
  static constexpr std::size_t kAlphabetSize = 256;
  static constexpr std::size_t kNoState = static_cast<std::size_t>(-1);

  struct State {
    std::array<std::size_t, kAlphabetSize> next{};
    std::size_t failure = 0;
    std::size_t output_link = kNoState;
    std::vector<std::size_t> terminal_pattern_indices;

    State();
  };

  void emit_state_matches(std::size_t state, std::size_t end,
                          std::vector<AhoCorasickMatch>& matches) const;

  std::vector<State> states_;
  std::vector<std::size_t> pattern_lengths_;
};

}  // namespace algorithms::strings
