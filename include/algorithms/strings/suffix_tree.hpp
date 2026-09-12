#pragma once

#include <cstddef>
#include <map>
#include <string_view>
#include <vector>

namespace algorithms::strings {

class SuffixTreeByteIndex {
 public:
  explicit SuffixTreeByteIndex(std::string_view text);

  [[nodiscard]] std::size_t text_size() const noexcept { return text_size_; }
  [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }

  // The empty pattern occurs at every text boundary, including text_size().
  [[nodiscard]] bool contains(std::string_view pattern) const;
  [[nodiscard]] std::size_t occurrence_count(std::string_view pattern) const;
  [[nodiscard]] std::vector<std::size_t> locate(std::string_view pattern) const;

  // Number of distinct non-empty byte substrings. Construction throws
  // std::overflow_error if this count is not representable by size_t.
  [[nodiscard]] std::size_t distinct_substring_count() const noexcept {
    return distinct_substring_count_;
  }

  // Expensive structural diagnostic. It verifies compressed-edge structure,
  // leaf suffix identities, cached leaf counts, suffix-link domains, and replays
  // every suffix through the tree. This is intentionally O(n^2) in the worst
  // case and is not part of query complexity claims.
  [[nodiscard]] bool valid_structure() const;

 private:
  static constexpr std::size_t kNoIndex = static_cast<std::size_t>(-1);
  static constexpr unsigned kSentinel = 256U;

  struct Node {
    std::size_t start = 0;
    std::size_t end = 0;
    bool open_end = false;
    std::size_t suffix_link = 0;
    std::size_t suffix_start = kNoIndex;
    std::size_t real_leaf_count = 0;
    std::map<unsigned, std::size_t> children;
  };

  std::vector<unsigned> symbols_;
  std::vector<Node> nodes_;
  std::size_t text_size_ = 0;
  std::size_t distinct_substring_count_ = 0;

  [[nodiscard]] std::size_t edge_length(std::size_t node,
                                        std::size_t current_end) const;
  [[nodiscard]] std::size_t create_node(std::size_t start, std::size_t end,
                                        bool open_end,
                                        std::size_t suffix_start);
  void extend(std::size_t position, std::size_t& active_node,
              std::size_t& active_edge, std::size_t& active_length,
              std::size_t& remaining);
  void finalize_metadata();

  [[nodiscard]] std::size_t match_node(std::string_view pattern) const;
  void collect_positions(std::size_t node,
                         std::vector<std::size_t>& output) const;
};

}  // namespace algorithms::strings
