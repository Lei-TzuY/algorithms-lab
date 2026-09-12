#include "algorithms/strings/suffix_tree.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace algorithms::strings {

SuffixTreeByteIndex::SuffixTreeByteIndex(std::string_view text)
    : text_size_(text.size()) {
  if (text.size() == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("suffix tree text length cannot accommodate sentinel");
  }
  symbols_.reserve(text.size() + 1U);
  for (char ch : text) {
    symbols_.push_back(static_cast<unsigned>(static_cast<unsigned char>(ch)));
  }
  symbols_.push_back(kSentinel);

  const std::size_t m = symbols_.size();
  if (m <= (std::numeric_limits<std::size_t>::max() - 1U) / 2U) {
    nodes_.reserve((2U * m) + 1U);
  }
  nodes_.push_back(Node{});  // root
  nodes_[0].suffix_link = 0;

  std::size_t active_node = 0;
  std::size_t active_edge = 0;
  std::size_t active_length = 0;
  std::size_t remaining = 0;
  for (std::size_t position = 0; position < symbols_.size(); ++position) {
    extend(position, active_node, active_edge, active_length, remaining);
  }
  finalize_metadata();
}

std::size_t SuffixTreeByteIndex::edge_length(std::size_t node,
                                              std::size_t current_end) const {
  const Node& n = nodes_[node];
  const std::size_t end = n.open_end ? current_end : n.end;
  if (end < n.start) {
    throw std::logic_error("suffix tree edge has reversed interval");
  }
  return (end - n.start) + 1U;
}

std::size_t SuffixTreeByteIndex::create_node(std::size_t start, std::size_t end,
                                              bool open_end,
                                              std::size_t suffix_start) {
  const std::size_t index = nodes_.size();
  nodes_.push_back(Node{start, end, open_end, 0U, suffix_start, 0U, {}});
  return index;
}

void SuffixTreeByteIndex::extend(std::size_t position, std::size_t& active_node,
                                 std::size_t& active_edge,
                                 std::size_t& active_length,
                                 std::size_t& remaining) {
  ++remaining;
  std::size_t last_created_internal = kNoIndex;

  while (remaining != 0U) {
    if (active_length == 0U) active_edge = position;
    const unsigned active_symbol = symbols_[active_edge];
    auto child_it = nodes_[active_node].children.find(active_symbol);

    if (child_it == nodes_[active_node].children.end()) {
      const std::size_t suffix_start = (position - remaining) + 1U;
      const std::size_t leaf = create_node(position, 0U, true, suffix_start);
      nodes_[active_node].children.emplace(active_symbol, leaf);
      if (last_created_internal != kNoIndex) {
        nodes_[last_created_internal].suffix_link = active_node;
        last_created_internal = kNoIndex;
      }
    } else {
      const std::size_t next = child_it->second;
      const std::size_t next_length = edge_length(next, position);
      if (active_length >= next_length) {
        active_edge += next_length;
        active_length -= next_length;
        active_node = next;
        continue;
      }

      const std::size_t comparison_index = nodes_[next].start + active_length;
      if (symbols_[comparison_index] == symbols_[position]) {
        if (last_created_internal != kNoIndex && active_node != 0U) {
          nodes_[last_created_internal].suffix_link = active_node;
          last_created_internal = kNoIndex;
        }
        ++active_length;
        break;
      }

      if (active_length == 0U) {
        throw std::logic_error("suffix tree split requested at edge start");
      }
      const std::size_t split_start = nodes_[next].start;
      const std::size_t split_end = split_start + active_length - 1U;
      const std::size_t split =
          create_node(split_start, split_end, false, kNoIndex);
      nodes_[active_node].children[active_symbol] = split;

      const std::size_t suffix_start = (position - remaining) + 1U;
      const std::size_t leaf = create_node(position, 0U, true, suffix_start);
      nodes_[split].children.emplace(symbols_[position], leaf);

      nodes_[next].start += active_length;
      nodes_[split].children.emplace(symbols_[nodes_[next].start], next);

      if (last_created_internal != kNoIndex) {
        nodes_[last_created_internal].suffix_link = split;
      }
      last_created_internal = split;
    }

    --remaining;
    if (active_node == 0U && active_length != 0U) {
      --active_length;
      active_edge = (position - remaining) + 1U;
    } else if (active_node != 0U) {
      active_node = nodes_[active_node].suffix_link;
    }
  }
}

void SuffixTreeByteIndex::finalize_metadata() {
  const std::size_t final_end = symbols_.size() - 1U;
  for (Node& node : nodes_) {
    if (node.open_end) {
      node.open_end = false;
      node.end = final_end;
    }
  }

  std::vector<std::pair<std::size_t, bool>> stack;
  if (nodes_.size() > std::numeric_limits<std::size_t>::max() / 2U) {
    throw std::overflow_error("suffix tree metadata traversal size overflow");
  }
  stack.reserve(nodes_.size() * 2U);
  stack.emplace_back(0U, false);
  while (!stack.empty()) {
    const auto [node_index, expanded] = stack.back();
    stack.pop_back();
    Node& node = nodes_[node_index];
    if (!expanded) {
      stack.emplace_back(node_index, true);
      for (const auto& [symbol, child] : node.children) {
        static_cast<void>(symbol);
        stack.emplace_back(child, false);
      }
      continue;
    }

    if (node.children.empty()) {
      node.real_leaf_count =
          (node.suffix_start != kNoIndex && node.suffix_start < text_size_) ? 1U
                                                                            : 0U;
    } else {
      std::size_t sum = 0U;
      for (const auto& [symbol, child] : node.children) {
        static_cast<void>(symbol);
        if (nodes_[child].real_leaf_count >
            std::numeric_limits<std::size_t>::max() - sum) {
          throw std::overflow_error("suffix tree occurrence count overflow");
        }
        sum += nodes_[child].real_leaf_count;
      }
      node.real_leaf_count = sum;
    }
  }

  std::size_t distinct = 0U;
  for (std::size_t i = 1U; i < nodes_.size(); ++i) {
    const Node& node = nodes_[i];
    if (node.start >= text_size_) continue;
    const std::size_t trimmed_end = std::min(node.end, text_size_ - 1U);
    if (trimmed_end < node.start) continue;
    const std::size_t contribution = (trimmed_end - node.start) + 1U;
    if (contribution > std::numeric_limits<std::size_t>::max() - distinct) {
      throw std::overflow_error("distinct substring count overflow");
    }
    distinct += contribution;
  }
  distinct_substring_count_ = distinct;
}

std::size_t SuffixTreeByteIndex::match_node(std::string_view pattern) const {
  if (pattern.empty()) return 0U;
  std::size_t node = 0U;
  std::size_t pattern_index = 0U;
  while (pattern_index < pattern.size()) {
    const unsigned symbol = static_cast<unsigned>(
        static_cast<unsigned char>(pattern[pattern_index]));
    const auto child_it = nodes_[node].children.find(symbol);
    if (child_it == nodes_[node].children.end()) return kNoIndex;
    const std::size_t child = child_it->second;
    const Node& edge = nodes_[child];
    for (std::size_t position = edge.start;
         position <= edge.end && pattern_index < pattern.size(); ++position) {
      if (symbols_[position] == kSentinel ||
          symbols_[position] != static_cast<unsigned>(
                                    static_cast<unsigned char>(pattern[pattern_index]))) {
        return kNoIndex;
      }
      ++pattern_index;
    }
    node = child;
  }
  return node;
}

bool SuffixTreeByteIndex::contains(std::string_view pattern) const {
  return match_node(pattern) != kNoIndex;
}

std::size_t SuffixTreeByteIndex::occurrence_count(
    std::string_view pattern) const {
  if (pattern.empty()) {
    if (text_size_ == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("empty-pattern occurrence count overflow");
    }
    return text_size_ + 1U;
  }
  const std::size_t node = match_node(pattern);
  return node == kNoIndex ? 0U : nodes_[node].real_leaf_count;
}

void SuffixTreeByteIndex::collect_positions(
    std::size_t node, std::vector<std::size_t>& output) const {
  std::vector<std::size_t> stack{node};
  while (!stack.empty()) {
    const std::size_t current_index = stack.back();
    stack.pop_back();
    const Node& current = nodes_[current_index];
    if (current.children.empty()) {
      if (current.suffix_start != kNoIndex && current.suffix_start < text_size_) {
        output.push_back(current.suffix_start);
      }
      continue;
    }
    for (const auto& [symbol, child] : current.children) {
      static_cast<void>(symbol);
      stack.push_back(child);
    }
  }
}

std::vector<std::size_t> SuffixTreeByteIndex::locate(
    std::string_view pattern) const {
  if (pattern.empty()) {
    std::vector<std::size_t> boundaries(text_size_ + 1U);
    for (std::size_t i = 0; i <= text_size_; ++i) boundaries[i] = i;
    return boundaries;
  }
  const std::size_t node = match_node(pattern);
  if (node == kNoIndex) return {};
  std::vector<std::size_t> positions;
  positions.reserve(nodes_[node].real_leaf_count);
  collect_positions(node, positions);
  std::sort(positions.begin(), positions.end());
  return positions;
}

bool SuffixTreeByteIndex::valid_structure() const {
  if (symbols_.size() != text_size_ + 1U || symbols_.empty() ||
      symbols_.back() != kSentinel || nodes_.empty()) {
    return false;
  }
  if (nodes_[0].suffix_link != 0U || nodes_[0].suffix_start != kNoIndex) {
    return false;
  }

  std::vector<std::uint8_t> seen(nodes_.size(), 0U);
  std::vector<std::size_t> depth(nodes_.size(), 0U);
  std::vector<std::size_t> stack{0U};
  std::vector<std::uint8_t> suffix_seen(symbols_.size(), 0U);
  while (!stack.empty()) {
    const std::size_t node_index = stack.back();
    stack.pop_back();
    if (node_index >= nodes_.size() || seen[node_index] != 0U) return false;
    seen[node_index] = 1U;
    const Node& node = nodes_[node_index];
    if (node_index != 0U) {
      if (node.start > node.end || node.end >= symbols_.size()) return false;
      if (!node.children.empty() && node.children.size() < 2U) return false;
      if (node.children.empty()) {
        if (node.suffix_start >= symbols_.size() ||
            suffix_seen[node.suffix_start] != 0U) {
          return false;
        }
        suffix_seen[node.suffix_start] = 1U;
      } else if (node.suffix_start != kNoIndex) {
        return false;
      }
      if (node.suffix_link >= nodes_.size()) return false;
    }
    for (const auto& [symbol, child] : node.children) {
      if (child >= nodes_.size() || nodes_[child].start >= symbols_.size() ||
          symbols_[nodes_[child].start] != symbol) {
        return false;
      }
      const Node& child_node = nodes_[child];
      const std::size_t child_edge_length =
          (child_node.end - child_node.start) + 1U;
      if (child_edge_length >
          std::numeric_limits<std::size_t>::max() - depth[node_index]) {
        return false;
      }
      depth[child] = depth[node_index] + child_edge_length;
      if (depth[child] > symbols_.size()) return false;
      stack.push_back(child);
    }
  }
  if (std::find(seen.begin(), seen.end(), 0U) != seen.end() ||
      std::find(suffix_seen.begin(), suffix_seen.end(), 0U) != suffix_seen.end()) {
    return false;
  }

  std::vector<std::size_t> counts(nodes_.size(), 0U);
  std::vector<std::pair<std::size_t, bool>> count_stack{{0U, false}};
  while (!count_stack.empty()) {
    const auto [node_index, expanded] = count_stack.back();
    count_stack.pop_back();
    const Node& node = nodes_[node_index];
    if (!expanded) {
      count_stack.emplace_back(node_index, true);
      for (const auto& [symbol, child] : node.children) {
        static_cast<void>(symbol);
        count_stack.emplace_back(child, false);
      }
      continue;
    }
    if (node.children.empty()) {
      counts[node_index] = node.suffix_start < text_size_ ? 1U : 0U;
    } else {
      for (const auto& [symbol, child] : node.children) {
        static_cast<void>(symbol);
        counts[node_index] += counts[child];
      }
    }
    if (counts[node_index] != node.real_leaf_count) return false;
  }

  std::vector<std::size_t> representative(nodes_.size(), kNoIndex);
  std::vector<std::pair<std::size_t, bool>> representative_stack{{0U, false}};
  while (!representative_stack.empty()) {
    const auto [node_index, expanded] = representative_stack.back();
    representative_stack.pop_back();
    const Node& node = nodes_[node_index];
    if (!expanded) {
      representative_stack.emplace_back(node_index, true);
      for (const auto& [symbol, child] : node.children) {
        static_cast<void>(symbol);
        representative_stack.emplace_back(child, false);
      }
      continue;
    }
    if (node.children.empty()) {
      representative[node_index] = node.suffix_start;
    } else {
      representative[node_index] =
          representative[node.children.begin()->second];
    }
  }

  for (std::size_t node_index = 1U; node_index < nodes_.size(); ++node_index) {
    const Node& node = nodes_[node_index];
    if (node.children.empty()) continue;
    if (node.end >= text_size_ || depth[node_index] == 0U ||
        representative[node_index] == kNoIndex) {
      return false;
    }
    const std::size_t suffix_start = representative[node_index] + 1U;
    std::size_t remaining = depth[node_index] - 1U;
    std::size_t position = suffix_start;
    std::size_t target = 0U;
    while (remaining != 0U) {
      if (position >= symbols_.size()) return false;
      const auto it = nodes_[target].children.find(symbols_[position]);
      if (it == nodes_[target].children.end()) return false;
      const std::size_t child = it->second;
      const Node& edge = nodes_[child];
      const std::size_t edge_len = (edge.end - edge.start) + 1U;
      if (edge_len > remaining) return false;
      for (std::size_t offset = 0U; offset < edge_len; ++offset) {
        if (position + offset >= symbols_.size() ||
            symbols_[edge.start + offset] != symbols_[position + offset]) {
          return false;
        }
      }
      position += edge_len;
      remaining -= edge_len;
      target = child;
    }
    if (nodes_[node_index].suffix_link != target) return false;
  }

  for (std::size_t suffix = 0; suffix < symbols_.size(); ++suffix) {
    std::size_t node = 0U;
    std::size_t position = suffix;
    while (position < symbols_.size()) {
      const auto it = nodes_[node].children.find(symbols_[position]);
      if (it == nodes_[node].children.end()) return false;
      node = it->second;
      const Node& edge = nodes_[node];
      for (std::size_t e = edge.start; e <= edge.end; ++e, ++position) {
        if (position >= symbols_.size() || symbols_[e] != symbols_[position]) {
          return false;
        }
      }
    }
    if (!nodes_[node].children.empty() || nodes_[node].suffix_start != suffix) {
      return false;
    }
  }
  return true;
}

}  // namespace algorithms::strings
