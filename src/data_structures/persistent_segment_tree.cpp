#include "algorithms/data_structures/persistent_segment_tree.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace algorithms::data_structures {

PersistentSegmentTree::PersistentSegmentTree(std::size_t element_count)
    : size_(element_count) {
  if (size_ == 0) {
    roots_.push_back(null_node());
    return;
  }
  roots_.push_back(build_zeros(0, size_));
}

PersistentSegmentTree::PersistentSegmentTree(
    const std::vector<std::int64_t>& values)
    : size_(values.size()) {
  if (size_ == 0) {
    roots_.push_back(null_node());
    return;
  }
  roots_.push_back(build_values(values, 0, size_));
}

std::size_t PersistentSegmentTree::build_zeros(std::size_t begin,
                                                std::size_t end) {
  if (end - begin == 1) {
    nodes_.push_back(Node{});
    return nodes_.size() - 1;
  }
  const std::size_t middle = begin + (end - begin) / 2;
  const std::size_t left = build_zeros(begin, middle);
  const std::size_t right = build_zeros(middle, end);
  nodes_.push_back(Node{0, left, right});
  return nodes_.size() - 1;
}

std::size_t PersistentSegmentTree::build_values(
    const std::vector<std::int64_t>& values, std::size_t begin,
    std::size_t end) {
  if (end - begin == 1) {
    nodes_.push_back(Node{values[begin], null_node(), null_node()});
    return nodes_.size() - 1;
  }
  const std::size_t middle = begin + (end - begin) / 2;
  const std::size_t left = build_values(values, begin, middle);
  const std::size_t right = build_values(values, middle, end);
  nodes_.push_back(
      Node{checked_add(nodes_[left].sum, nodes_[right].sum), left, right});
  return nodes_.size() - 1;
}

PersistentSegmentTree::Version PersistentSegmentTree::assign(
    Version base_version, std::size_t index, std::int64_t value) {
  validate_version(base_version);
  validate_index(index);

  const std::size_t path_nodes = update_path_nodes();
  if (nodes_.size() > std::numeric_limits<std::size_t>::max() - path_nodes ||
      roots_.size() == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("persistent segment tree storage exhausted");
  }

  // Reserve both containers before making any logical mutation. For these
  // trivially copyable element types, a reserve failure leaves the vectors
  // unchanged. After successful reserve, path-copy push_back cannot allocate.
  nodes_.reserve(nodes_.size() + path_nodes);
  roots_.reserve(roots_.size() + 1);

  const std::size_t old_node_count = nodes_.size();
  try {
    const std::size_t new_root =
        update_node(roots_[base_version], 0, size_, index, value);
    roots_.push_back(new_root);
  } catch (...) {
    nodes_.resize(old_node_count);
    throw;
  }
  return roots_.size() - 1;
}

std::size_t PersistentSegmentTree::update_node(
    std::size_t node, std::size_t begin, std::size_t end, std::size_t index,
    std::int64_t value) {
  if (end - begin == 1) {
    nodes_.push_back(Node{value, null_node(), null_node()});
    return nodes_.size() - 1;
  }

  const Node original = nodes_[node];
  const std::size_t middle = begin + (end - begin) / 2;
  std::size_t left = original.left;
  std::size_t right = original.right;
  if (index < middle) {
    left = update_node(original.left, begin, middle, index, value);
  } else {
    right = update_node(original.right, middle, end, index, value);
  }
  nodes_.push_back(
      Node{checked_add(nodes_[left].sum, nodes_[right].sum), left, right});
  return nodes_.size() - 1;
}

std::int64_t PersistentSegmentTree::range_sum(Version version,
                                               std::size_t begin,
                                               std::size_t end) const {
  validate_version(version);
  if (begin > end || end > size_) {
    throw std::out_of_range("persistent segment tree range out of range");
  }
  if (begin == end) {
    return 0;
  }

  std::vector<std::int64_t> terms;
  collect_terms(roots_[version], 0, size_, begin, end, terms);
  return checked_sum_terms(std::move(terms));
}

void PersistentSegmentTree::collect_terms(
    std::size_t node, std::size_t begin, std::size_t end,
    std::size_t query_begin, std::size_t query_end,
    std::vector<std::int64_t>& terms) const {
  if (query_begin <= begin && end <= query_end) {
    terms.push_back(nodes_[node].sum);
    return;
  }
  const std::size_t middle = begin + (end - begin) / 2;
  if (query_begin < middle) {
    collect_terms(nodes_[node].left, begin, middle, query_begin, query_end,
                  terms);
  }
  if (middle < query_end) {
    collect_terms(nodes_[node].right, middle, end, query_begin, query_end,
                  terms);
  }
}

std::int64_t PersistentSegmentTree::checked_add(std::int64_t left,
                                                 std::int64_t right) {
  if ((right > 0 &&
       left > std::numeric_limits<std::int64_t>::max() - right) ||
      (right < 0 &&
       left < std::numeric_limits<std::int64_t>::min() - right)) {
    throw std::overflow_error("persistent segment tree sum overflow");
  }
  return left + right;
}

std::int64_t PersistentSegmentTree::checked_sum_terms(
    std::vector<std::int64_t> terms) {
  std::vector<std::int64_t> positive;
  std::vector<std::int64_t> negative;
  positive.reserve(terms.size());
  negative.reserve(terms.size());
  for (const auto term : terms) {
    if (term < 0) {
      negative.push_back(term);
    } else {
      positive.push_back(term);
    }
  }

  std::int64_t sum = 0;
  while (!positive.empty() || !negative.empty()) {
    if (sum >= 0 && !negative.empty()) {
      sum = checked_add(sum, negative.back());
      negative.pop_back();
    } else if (sum < 0 && !positive.empty()) {
      sum = checked_add(sum, positive.back());
      positive.pop_back();
    } else if (!positive.empty()) {
      sum = checked_add(sum, positive.back());
      positive.pop_back();
    } else {
      sum = checked_add(sum, negative.back());
      negative.pop_back();
    }
  }
  return sum;
}

void PersistentSegmentTree::validate_version(Version version) const {
  if (version >= roots_.size()) {
    throw std::out_of_range("persistent segment tree version out of range");
  }
}

void PersistentSegmentTree::validate_index(std::size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("persistent segment tree index out of range");
  }
}

std::size_t PersistentSegmentTree::update_path_nodes() const noexcept {
  if (size_ == 0) {
    return 0;
  }
  std::size_t length = size_;
  std::size_t count = 1;
  while (length > 1) {
    length = (length + 1) / 2;
    ++count;
  }
  return count;
}

}  // namespace algorithms::data_structures
