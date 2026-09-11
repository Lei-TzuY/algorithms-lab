#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct PalindromeRecord {
  std::size_t node_id{};
  std::size_t length{};
  std::size_t first_start{};
  std::size_t first_end{};
  std::size_t occurrences{};
  std::size_t suffix_link_length{};

  friend bool operator==(const PalindromeRecord&, const PalindromeRecord&) = default;
};

class PalindromicTree {
 public:
  PalindromicTree();
  explicit PalindromicTree(std::string_view bytes);

  // Append one byte and return the stable node id for the longest palindromic
  // suffix of the resulting prefix.
  std::size_t append(std::uint8_t byte);

  [[nodiscard]] std::size_t length() const noexcept;
  [[nodiscard]] std::size_t distinct_palindrome_count() const noexcept;
  [[nodiscard]] std::size_t longest_suffix_length() const noexcept;

  // Returns one record per distinct non-empty palindrome in stable node-id
  // (creation) order. Occurrence counts are exact over the current text.
  [[nodiscard]] std::vector<PalindromeRecord> palindromes() const;

  // Full diagnostic replay of list-independent Eertree invariants. Intended
  // for tests/teaching rather than as an operation-complexity primitive.
  [[nodiscard]] bool valid_structure() const;

 private:
  struct Node {
    std::ptrdiff_t palindrome_length{};
    std::size_t suffix_link{};
    std::array<std::size_t, 256U> transitions{};
    std::size_t first_end{};
    std::size_t terminal_occurrences{};
  };

  [[nodiscard]] std::size_t find_extendable_suffix(std::size_t node_id,
                                                    std::size_t position,
                                                    std::uint8_t byte) const;

  std::vector<std::uint8_t> text_;
  std::vector<Node> nodes_;
  std::size_t longest_suffix_node_{1U};
};

}  // namespace algorithms::strings
