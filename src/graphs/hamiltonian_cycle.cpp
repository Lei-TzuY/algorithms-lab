#include "algorithms/graphs/hamiltonian_cycle.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {
namespace {

struct WideCost {
  bool negative{false};
  std::uint64_t high{0};
  std::uint64_t low{0};
};

[[nodiscard]] std::uint64_t magnitude_of(Weight value) {
  if (value >= 0) {
    return static_cast<std::uint64_t>(value);
  }
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] int compare_magnitude(const WideCost& left,
                                    const WideCost& right) {
  if (left.high != right.high) {
    return left.high < right.high ? -1 : 1;
  }
  if (left.low != right.low) {
    return left.low < right.low ? -1 : 1;
  }
  return 0;
}

[[nodiscard]] int compare(const WideCost& left, const WideCost& right) {
  if (left.negative != right.negative) {
    return left.negative ? -1 : 1;
  }
  const int magnitude = compare_magnitude(left, right);
  return left.negative ? -magnitude : magnitude;
}

void normalize(WideCost& value) {
  if (value.high == 0U && value.low == 0U) {
    value.negative = false;
  }
}

void add_magnitude(WideCost& value, std::uint64_t addend) {
  const std::uint64_t old_low = value.low;
  value.low += addend;
  const std::uint64_t carry = value.low < old_low ? 1U : 0U;
  if (carry != 0U && value.high == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("Hamiltonian DP wide accumulator overflow");
  }
  value.high += carry;
}

void subtract_magnitude(WideCost& value, std::uint64_t subtrahend) {
  if (value.high == 0U) {
    value.low -= subtrahend;
    return;
  }
  if (value.low < subtrahend) {
    --value.high;
  }
  value.low -= subtrahend;
}

[[nodiscard]] WideCost add_weight(WideCost value, Weight weight) {
  const bool addend_negative = weight < 0;
  const std::uint64_t addend_magnitude = magnitude_of(weight);
  if (addend_magnitude == 0U) {
    return value;
  }

  if (value.high == 0U && value.low == 0U) {
    value.negative = addend_negative;
    value.low = addend_magnitude;
    return value;
  }

  if (value.negative == addend_negative) {
    add_magnitude(value, addend_magnitude);
    return value;
  }

  if (value.high != 0U || value.low > addend_magnitude) {
    subtract_magnitude(value, addend_magnitude);
    normalize(value);
    return value;
  }
  if (value.low == addend_magnitude) {
    return WideCost{};
  }

  value.negative = addend_negative;
  value.high = 0U;
  value.low = addend_magnitude - value.low;
  return value;
}

[[nodiscard]] Weight narrow_cost(const WideCost& value) {
  if (value.high != 0U) {
    throw std::overflow_error("Hamiltonian optimum is not int64-representable");
  }
  if (!value.negative) {
    if (value.low > static_cast<std::uint64_t>(
                        std::numeric_limits<Weight>::max())) {
      throw std::overflow_error("Hamiltonian optimum is not int64-representable");
    }
    return static_cast<Weight>(value.low);
  }

  constexpr std::uint64_t kMinMagnitude = std::uint64_t{1} << 63U;
  if (value.low > kMinMagnitude) {
    throw std::overflow_error("Hamiltonian optimum is not int64-representable");
  }
  if (value.low == kMinMagnitude) {
    return std::numeric_limits<Weight>::min();
  }
  return -static_cast<Weight>(value.low);
}

struct CanonicalArc {
  bool exists{false};
  Weight weight{};
  std::size_t adjacency_index{};
};

struct DpState {
  WideCost cost{};
  std::int16_t parent{-1};
  bool reachable{false};
};

}  // namespace

std::optional<HamiltonianCycleResult> minimum_directed_hamiltonian_cycle(
    const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument("Hamiltonian cycle requires a directed graph");
  }

  const std::size_t n = graph.vertex_count();
  if (n > 16U) {
    throw std::length_error("Hamiltonian Held-Karp baseline supports at most 16 vertices");
  }
  if (n == 0U) {
    return HamiltonianCycleResult{};
  }
  if (n == 1U) {
    HamiltonianCycleResult result;
    result.cycle_vertices = {0U};
    return result;
  }

  std::vector<CanonicalArc> arcs(n * n);
  for (Vertex from = 0; from < n; ++from) {
    const auto& neighbors = graph.neighbors(from);
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
      const Edge& edge = neighbors[index];
      if (edge.to == from) {
        continue;
      }
      CanonicalArc& chosen = arcs[from * n + edge.to];
      if (!chosen.exists || edge.weight < chosen.weight ||
          (edge.weight == chosen.weight && index < chosen.adjacency_index)) {
        chosen.exists = true;
        chosen.weight = edge.weight;
        chosen.adjacency_index = index;
      }
    }
  }

  const std::size_t non_root = n - 1U;
  const std::size_t mask_count = std::size_t{1} << non_root;
  std::vector<DpState> dp(mask_count * n);
  std::size_t reachable_states = 0U;

  for (Vertex vertex = 1U; vertex < n; ++vertex) {
    const CanonicalArc& edge = arcs[vertex];
    if (!edge.exists) {
      continue;
    }
    const std::size_t mask = std::size_t{1} << (vertex - 1U);
    DpState& state = dp[mask * n + vertex];
    state.reachable = true;
    state.cost = add_weight(WideCost{}, edge.weight);
    state.parent = 0;
    ++reachable_states;
  }

  for (std::size_t mask = 1U; mask < mask_count; ++mask) {
    for (Vertex last = 1U; last < n; ++last) {
      const std::size_t last_bit = std::size_t{1} << (last - 1U);
      if ((mask & last_bit) == 0U) {
        continue;
      }
      const DpState& state = dp[mask * n + last];
      if (!state.reachable) {
        continue;
      }
      for (Vertex next = 1U; next < n; ++next) {
        const std::size_t next_bit = std::size_t{1} << (next - 1U);
        if ((mask & next_bit) != 0U) {
          continue;
        }
        const CanonicalArc& edge = arcs[last * n + next];
        if (!edge.exists) {
          continue;
        }
        const WideCost candidate = add_weight(state.cost, edge.weight);
        DpState& destination = dp[(mask | next_bit) * n + next];
        const bool improves = !destination.reachable ||
                              compare(candidate, destination.cost) < 0 ||
                              (compare(candidate, destination.cost) == 0 &&
                               static_cast<std::int16_t>(last) < destination.parent);
        if (improves) {
          if (!destination.reachable) {
            ++reachable_states;
          }
          destination.reachable = true;
          destination.cost = candidate;
          destination.parent = static_cast<std::int16_t>(last);
        }
      }
    }
  }

  const std::size_t full_mask = mask_count - 1U;
  bool found = false;
  WideCost best_cost{};
  Vertex best_last = 0U;
  for (Vertex last = 1U; last < n; ++last) {
    const DpState& state = dp[full_mask * n + last];
    const CanonicalArc& closing = arcs[last * n];
    if (!state.reachable || !closing.exists) {
      continue;
    }
    const WideCost candidate = add_weight(state.cost, closing.weight);
    if (!found || compare(candidate, best_cost) < 0 ||
        (compare(candidate, best_cost) == 0 && last < best_last)) {
      found = true;
      best_cost = candidate;
      best_last = last;
    }
  }
  if (!found) {
    return std::nullopt;
  }

  std::vector<Vertex> reverse_path;
  reverse_path.reserve(non_root);
  std::size_t mask = full_mask;
  Vertex current = best_last;
  while (current != 0U) {
    reverse_path.push_back(current);
    const DpState& state = dp[mask * n + current];
    if (state.parent < 0) {
      throw std::logic_error("Hamiltonian DP reconstruction lost its parent");
    }
    const Vertex parent = static_cast<Vertex>(state.parent);
    mask &= ~(std::size_t{1} << (current - 1U));
    current = parent;
  }
  std::reverse(reverse_path.begin(), reverse_path.end());

  HamiltonianCycleResult result;
  result.total_weight = narrow_cost(best_cost);
  result.reachable_dp_states = reachable_states;
  result.cycle_vertices.reserve(n + 1U);
  result.cycle_vertices.push_back(0U);
  result.cycle_vertices.insert(result.cycle_vertices.end(), reverse_path.begin(),
                               reverse_path.end());
  result.cycle_vertices.push_back(0U);
  result.edges.reserve(n);
  for (std::size_t i = 0; i + 1U < result.cycle_vertices.size(); ++i) {
    const Vertex from = result.cycle_vertices[i];
    const Vertex to = result.cycle_vertices[i + 1U];
    const CanonicalArc& edge = arcs[from * n + to];
    if (!edge.exists) {
      throw std::logic_error("Hamiltonian reconstruction references a missing edge");
    }
    result.edges.push_back(
        HamiltonianCycleEdge{from, edge.adjacency_index, to, edge.weight});
  }
  return result;
}

}  // namespace algorithms::graphs
