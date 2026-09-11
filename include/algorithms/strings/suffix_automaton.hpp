#pragma once

#include <cstddef>
#include <map>
#include <string_view>
#include <vector>

namespace algorithms::strings {

class SuffixAutomatonByteIndex {
 public:
  explicit SuffixAutomatonByteIndex(std::string_view text);

  [[nodiscard]] bool contains(std::string_view pattern) const;
  [[nodiscard]] std::size_t occurrence_count(std::string_view pattern) const;
  [[nodiscard]] std::size_t distinct_substring_count() const noexcept;
  [[nodiscard]] std::size_t state_count() const noexcept;
  [[nodiscard]] std::size_t text_length() const noexcept;

 private:
  static constexpr std::size_t kNoState = static_cast<std::size_t>(-1);

  struct State {
    std::size_t length = 0;
    std::size_t link = kNoState;
    std::size_t occurrences = 0;
    std::map<unsigned char, std::size_t> next;
  };

  [[nodiscard]] std::size_t find_state(std::string_view pattern) const;
  void extend(unsigned char byte);
  void finalize_occurrences_and_distinct_count();

  std::vector<State> states_;
  std::size_t last_ = 0;
  std::size_t text_length_ = 0;
  std::size_t distinct_substring_count_ = 0;
};

}  // namespace algorithms::strings
