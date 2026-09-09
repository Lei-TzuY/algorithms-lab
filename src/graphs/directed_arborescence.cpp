#include "algorithms/graphs/directed_arborescence.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

class ExactInteger {
 public:
  ExactInteger() = default;

  explicit ExactInteger(std::int64_t value) {
    if (value == 0) {
      return;
    }
    negative_ = value < 0;
    const std::uint64_t magnitude =
        negative_ ? static_cast<std::uint64_t>(-(value + 1)) + 1U
                  : static_cast<std::uint64_t>(value);
    limbs_.push_back(static_cast<std::uint32_t>(magnitude & 0xffffffffULL));
    const std::uint32_t high = static_cast<std::uint32_t>(magnitude >> 32U);
    if (high != 0U) {
      limbs_.push_back(high);
    }
  }

  friend bool operator<(const ExactInteger& lhs, const ExactInteger& rhs) {
    if (lhs.negative_ != rhs.negative_) {
      return lhs.negative_;
    }
    const int magnitude_comparison = compare_magnitude(lhs, rhs);
    return lhs.negative_ ? magnitude_comparison > 0 : magnitude_comparison < 0;
  }

  friend ExactInteger operator+(const ExactInteger& lhs,
                                const ExactInteger& rhs) {
    if (lhs.negative_ == rhs.negative_) {
      ExactInteger result;
      result.negative_ = lhs.negative_;
      result.limbs_ = add_magnitude(lhs.limbs_, rhs.limbs_);
      result.normalize();
      return result;
    }

    const int comparison = compare_magnitude(lhs, rhs);
    if (comparison == 0) {
      return ExactInteger{};
    }
    ExactInteger result;
    if (comparison > 0) {
      result.negative_ = lhs.negative_;
      result.limbs_ = subtract_magnitude(lhs.limbs_, rhs.limbs_);
    } else {
      result.negative_ = rhs.negative_;
      result.limbs_ = subtract_magnitude(rhs.limbs_, lhs.limbs_);
    }
    result.normalize();
    return result;
  }

  friend ExactInteger operator-(const ExactInteger& lhs,
                                const ExactInteger& rhs) {
    ExactInteger negated = rhs;
    if (!negated.limbs_.empty()) {
      negated.negative_ = !negated.negative_;
    }
    return lhs + negated;
  }

  [[nodiscard]] std::int64_t to_int64() const {
    if (limbs_.size() > 2U) {
      throw std::overflow_error("arborescence total cost is not int64-representable");
    }
    std::uint64_t magnitude = 0;
    if (!limbs_.empty()) {
      magnitude = static_cast<std::uint64_t>(limbs_[0]);
    }
    if (limbs_.size() == 2U) {
      magnitude |= static_cast<std::uint64_t>(limbs_[1]) << 32U;
    }

    const std::uint64_t max_positive =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const std::uint64_t max_negative_magnitude = max_positive + 1U;
    if (!negative_) {
      if (magnitude > max_positive) {
        throw std::overflow_error("arborescence total cost is not int64-representable");
      }
      return static_cast<std::int64_t>(magnitude);
    }
    if (magnitude > max_negative_magnitude) {
      throw std::overflow_error("arborescence total cost is not int64-representable");
    }
    if (magnitude == max_negative_magnitude) {
      return std::numeric_limits<std::int64_t>::min();
    }
    return -static_cast<std::int64_t>(magnitude);
  }

 private:
  static int compare_magnitude(const ExactInteger& lhs,
                               const ExactInteger& rhs) {
    if (lhs.limbs_.size() != rhs.limbs_.size()) {
      return lhs.limbs_.size() < rhs.limbs_.size() ? -1 : 1;
    }
    for (std::size_t i = lhs.limbs_.size(); i > 0U; --i) {
      const std::uint32_t left = lhs.limbs_[i - 1U];
      const std::uint32_t right = rhs.limbs_[i - 1U];
      if (left != right) {
        return left < right ? -1 : 1;
      }
    }
    return 0;
  }

  static std::vector<std::uint32_t> add_magnitude(
      const std::vector<std::uint32_t>& lhs,
      const std::vector<std::uint32_t>& rhs) {
    const std::size_t size = std::max(lhs.size(), rhs.size());
    std::vector<std::uint32_t> result;
    result.reserve(size + 1U);
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < size; ++i) {
      const std::uint64_t left =
          i < lhs.size() ? static_cast<std::uint64_t>(lhs[i]) : 0U;
      const std::uint64_t right =
          i < rhs.size() ? static_cast<std::uint64_t>(rhs[i]) : 0U;
      const std::uint64_t sum = left + right + carry;
      result.push_back(static_cast<std::uint32_t>(sum & 0xffffffffULL));
      carry = sum >> 32U;
    }
    if (carry != 0U) {
      result.push_back(static_cast<std::uint32_t>(carry));
    }
    return result;
  }

  static std::vector<std::uint32_t> subtract_magnitude(
      const std::vector<std::uint32_t>& larger,
      const std::vector<std::uint32_t>& smaller) {
    std::vector<std::uint32_t> result(larger.size(), 0U);
    std::uint64_t borrow = 0;
    constexpr std::uint64_t base = 1ULL << 32U;
    for (std::size_t i = 0; i < larger.size(); ++i) {
      const std::uint64_t left = static_cast<std::uint64_t>(larger[i]);
      const std::uint64_t right =
          (i < smaller.size() ? static_cast<std::uint64_t>(smaller[i]) : 0U) +
          borrow;
      if (left >= right) {
        result[i] = static_cast<std::uint32_t>(left - right);
        borrow = 0;
      } else {
        result[i] = static_cast<std::uint32_t>(base + left - right);
        borrow = 1;
      }
    }
    return result;
  }

  void normalize() {
    while (!limbs_.empty() && limbs_.back() == 0U) {
      limbs_.pop_back();
    }
    if (limbs_.empty()) {
      negative_ = false;
    }
  }

  bool negative_ = false;
  std::vector<std::uint32_t> limbs_;
};

struct LevelEdge {
  std::size_t from;
  std::size_t to;
  ExactInteger cost;
  std::size_t original_index;
  std::size_t parent_edge_index;
};

[[nodiscard]] bool edge_is_better(const LevelEdge& candidate,
                                  const LevelEdge& current) {
  if (candidate.cost < current.cost) {
    return true;
  }
  if (current.cost < candidate.cost) {
    return false;
  }
  return candidate.original_index < current.original_index;
}

[[nodiscard]] std::vector<std::size_t> solve_level(
    std::size_t vertex_count, std::size_t root,
    const std::vector<LevelEdge>& edges) {
  if (vertex_count <= 1U) {
    return {};
  }

  std::vector<std::optional<std::size_t>> incoming(vertex_count);
  for (std::size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
    const LevelEdge& edge = edges[edge_index];
    if (edge.from == edge.to || edge.to == root) {
      continue;
    }
    if (!incoming[edge.to].has_value() ||
        edge_is_better(edge, edges[*incoming[edge.to]])) {
      incoming[edge.to] = edge_index;
    }
  }
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex != root && !incoming[vertex].has_value()) {
      throw std::invalid_argument("root cannot reach every arborescence vertex");
    }
  }

  const std::size_t npos = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> component(vertex_count, npos);
  std::vector<std::size_t> visited_from(vertex_count, npos);
  std::size_t cycle_count = 0;

  for (std::size_t start = 0; start < vertex_count; ++start) {
    std::size_t vertex = start;
    while (vertex != root && visited_from[vertex] != start &&
           component[vertex] == npos) {
      visited_from[vertex] = start;
      vertex = edges[*incoming[vertex]].from;
    }
    if (vertex == root || component[vertex] != npos ||
        visited_from[vertex] != start) {
      continue;
    }

    component[vertex] = cycle_count;
    for (std::size_t member = edges[*incoming[vertex]].from; member != vertex;
         member = edges[*incoming[member]].from) {
      component[member] = cycle_count;
    }
    ++cycle_count;
  }

  if (cycle_count == 0U) {
    std::vector<std::size_t> selected;
    selected.reserve(vertex_count - 1U);
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
      if (vertex != root) {
        selected.push_back(*incoming[vertex]);
      }
    }
    return selected;
  }

  std::size_t component_count = cycle_count;
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (component[vertex] == npos) {
      component[vertex] = component_count;
      ++component_count;
    }
  }

  std::vector<LevelEdge> contracted_edges;
  contracted_edges.reserve(edges.size());
  for (std::size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
    const LevelEdge& edge = edges[edge_index];
    const std::size_t from_component = component[edge.from];
    const std::size_t to_component = component[edge.to];
    if (from_component == to_component) {
      continue;
    }

    ExactInteger adjusted_cost = edge.cost;
    if (edge.to != root) {
      adjusted_cost = adjusted_cost - edges[*incoming[edge.to]].cost;
    }
    contracted_edges.push_back(LevelEdge{from_component, to_component,
                                         std::move(adjusted_cost),
                                         edge.original_index, edge_index});
  }

  const std::vector<std::size_t> contracted_selection =
      solve_level(component_count, component[root], contracted_edges);

  std::vector<std::optional<std::size_t>> chosen = incoming;
  chosen[root].reset();
  for (const std::size_t contracted_edge_index : contracted_selection) {
    const std::size_t parent_edge_index =
        contracted_edges[contracted_edge_index].parent_edge_index;
    const std::size_t target = edges[parent_edge_index].to;
    chosen[target] = parent_edge_index;
  }

  std::vector<std::size_t> selected;
  selected.reserve(vertex_count - 1U);
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex != root) {
      selected.push_back(*chosen[vertex]);
    }
  }
  return selected;
}

void validate_reachability(std::size_t vertex_count, std::size_t root,
                           std::span<const DirectedArborescenceEdge> edges) {
  std::vector<std::vector<std::size_t>> adjacency(vertex_count);
  for (const DirectedArborescenceEdge& edge : edges) {
    if (edge.from >= vertex_count || edge.to >= vertex_count) {
      throw std::out_of_range("arborescence edge endpoint out of range");
    }
    if (edge.from != edge.to) {
      adjacency[edge.from].push_back(edge.to);
    }
  }

  std::vector<bool> visited(vertex_count, false);
  std::queue<std::size_t> queue;
  visited[root] = true;
  queue.push(root);
  while (!queue.empty()) {
    const std::size_t from = queue.front();
    queue.pop();
    for (const std::size_t to : adjacency[from]) {
      if (!visited[to]) {
        visited[to] = true;
        queue.push(to);
      }
    }
  }
  if (std::find(visited.begin(), visited.end(), false) != visited.end()) {
    throw std::invalid_argument("root cannot reach every arborescence vertex");
  }
}

}  // namespace

MinimumArborescenceResult chu_liu_edmonds_minimum_arborescence(
    std::size_t vertex_count,
    std::span<const DirectedArborescenceEdge> edges,
    ArborescenceVertex root) {
  if (root >= vertex_count) {
    throw std::out_of_range("arborescence root out of range");
  }
  validate_reachability(vertex_count, root, edges);

  std::vector<LevelEdge> level_edges;
  level_edges.reserve(edges.size());
  for (std::size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
    const DirectedArborescenceEdge& edge = edges[edge_index];
    if (edge.from == edge.to) {
      continue;
    }
    level_edges.push_back(LevelEdge{edge.from, edge.to, ExactInteger(edge.cost),
                                    edge_index, edge_index});
  }

  const std::vector<std::size_t> selected_level_edges =
      solve_level(vertex_count, root, level_edges);

  std::vector<std::optional<std::size_t>> incoming(vertex_count);
  for (const std::size_t level_edge_index : selected_level_edges) {
    const LevelEdge& level_edge = level_edges[level_edge_index];
    incoming[level_edge.to] = level_edge.original_index;
  }

  std::vector<std::size_t> edge_indices;
  edge_indices.reserve(vertex_count > 0U ? vertex_count - 1U : 0U);
  ExactInteger exact_total;
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex == root) {
      continue;
    }
    if (!incoming[vertex].has_value()) {
      throw std::logic_error("arborescence reconstruction lost an incoming edge");
    }
    const std::size_t edge_index = *incoming[vertex];
    edge_indices.push_back(edge_index);
    exact_total = exact_total + ExactInteger(edges[edge_index].cost);
  }

  return MinimumArborescenceResult{exact_total.to_int64(),
                                   std::move(edge_indices),
                                   std::move(incoming)};
}

}  // namespace algorithms::graphs
