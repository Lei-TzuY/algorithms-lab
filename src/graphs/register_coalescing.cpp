#include "algorithms/graphs/register_coalescing.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

struct LocationLess {
  bool operator()(const SsaCopyLocation& first,
                  const SsaCopyLocation& second) const {
    if (first.kind != second.kind) {
      return first.kind == SsaCopyLocationKind::value;
    }
    if (first.kind == SsaCopyLocationKind::value) {
      return std::tie(first.value.variable, first.value.version) <
             std::tie(second.value.variable, second.value.version);
    }
    return first.temporary < second.temporary;
  }
};

[[nodiscard]] std::size_t location_index(
    const std::vector<SsaCopyLocation>& locations,
    const SsaCopyLocation location) {
  const auto found = std::lower_bound(locations.begin(), locations.end(), location,
                                      LocationLess{});
  if (found == locations.end() || *found != location) {
    throw std::logic_error("copy-coalescing location universe is incomplete");
  }
  return static_cast<std::size_t>(found - locations.begin());
}

[[nodiscard]] std::size_t find_root(std::vector<std::size_t>& parent,
                                    const std::size_t node) {
  std::size_t root = node;
  while (parent[root] != root) {
    root = parent[root];
  }
  std::size_t current = node;
  while (parent[current] != current) {
    const std::size_t next = parent[current];
    parent[current] = root;
    current = next;
  }
  return root;
}

[[nodiscard]] bool classes_interfere(
    std::vector<std::size_t>& parent,
    const std::vector<std::pair<std::size_t, std::size_t>>& interference,
    const std::size_t first_root, const std::size_t second_root) {
  for (const auto& [first, second] : interference) {
    const std::size_t first_class = find_root(parent, first);
    const std::size_t second_class = find_root(parent, second);
    if ((first_class == first_root && second_class == second_root) ||
        (first_class == second_root && second_class == first_root)) {
      return true;
    }
  }
  return false;
}

void merge_roots(std::vector<std::size_t>& parent,
                 std::vector<std::size_t>& class_size,
                 std::size_t first, std::size_t second) {
  if (class_size[first] < class_size[second] ||
      (class_size[first] == class_size[second] && second < first)) {
    std::swap(first, second);
  }
  parent[second] = first;
  class_size[first] += class_size[second];
}

struct MoveReference {
  Vertex block{0U};
  PhiFreeOperationKind kind{PhiFreeOperationKind::entry_move};
  std::size_t index{0U};
  SsaScheduledMove move;
};

[[nodiscard]] std::vector<MoveReference> move_references(
    const OutOfSsaProgram& program) {
  std::vector<MoveReference> result;
  for (Vertex block = 0U; block < program.blocks.size(); ++block) {
    for (std::size_t index = 0U;
         index < program.blocks[block].entry_moves.size(); ++index) {
      result.push_back(MoveReference{block, PhiFreeOperationKind::entry_move,
                                     index,
                                     program.blocks[block].entry_moves[index]});
    }
    for (std::size_t index = 0U;
         index < program.blocks[block].exit_moves.size(); ++index) {
      result.push_back(MoveReference{block, PhiFreeOperationKind::exit_move,
                                     index,
                                     program.blocks[block].exit_moves[index]});
    }
  }
  return result;
}

}  // namespace

PhiFreeCoalescedRegisterAllocation coalesce_phi_free_registers(
    const OutOfSsaProgram& program, const std::size_t register_budget) {
  const PhiFreeRegisterAllocation baseline =
      allocate_phi_free_registers(program, register_budget);

  PhiFreeCoalescedRegisterAllocation result;
  result.register_budget = register_budget;
  result.locations = baseline.locations;
  result.original_interference_edges = baseline.interference_edges;

  const std::size_t location_count = result.locations.size();
  std::vector<std::pair<std::size_t, std::size_t>> interference;
  interference.reserve(baseline.interference_edges.size());
  for (const RegisterInterferenceEdge& edge : baseline.interference_edges) {
    std::size_t first = location_index(result.locations, edge.first);
    std::size_t second = location_index(result.locations, edge.second);
    if (second < first) {
      std::swap(first, second);
    }
    if (first == second) {
      throw std::logic_error("copy-coalescing interference self-edge");
    }
    interference.emplace_back(first, second);
  }

  std::vector<std::size_t> parent(location_count);
  std::iota(parent.begin(), parent.end(), 0U);
  std::vector<std::size_t> class_size(location_count, 1U);
  const std::vector<MoveReference> moves = move_references(program);
  result.preferences.reserve(moves.size());

  for (const MoveReference& reference : moves) {
    const std::size_t source =
        location_index(result.locations, reference.move.source);
    const std::size_t destination =
        location_index(result.locations, reference.move.destination);
    const std::size_t source_root = find_root(parent, source);
    const std::size_t destination_root = find_root(parent, destination);

    CopyCoalescingDecisionKind decision =
        CopyCoalescingDecisionKind::already_coalesced;
    if (source_root != destination_root) {
      if (classes_interfere(parent, interference, source_root,
                            destination_root)) {
        decision = CopyCoalescingDecisionKind::blocked_by_interference;
      } else {
        merge_roots(parent, class_size, source_root, destination_root);
        decision = CopyCoalescingDecisionKind::merged;
      }
    }
    result.preferences.push_back(CopyPreferenceDecision{
        reference.block, reference.kind, reference.index, reference.move,
        decision});
  }

  for (std::size_t location = 0U; location < location_count; ++location) {
    static_cast<void>(find_root(parent, location));
  }

  std::vector<std::size_t> root_to_class(location_count, location_count);
  std::vector<std::size_t> location_class(location_count, location_count);
  for (std::size_t location = 0U; location < location_count; ++location) {
    const std::size_t root = parent[location];
    if (root_to_class[root] == location_count) {
      root_to_class[root] = result.classes.size();
      result.classes.push_back(
          RegisterCoalescingClass{result.classes.size(), {}, std::nullopt});
    }
    const std::size_t class_id = root_to_class[root];
    location_class[location] = class_id;
    result.classes[class_id].members.push_back(result.locations[location]);
  }

  std::set<std::pair<std::size_t, std::size_t>> quotient_edges;
  for (const auto& [first, second] : interference) {
    std::size_t first_class = location_class[first];
    std::size_t second_class = location_class[second];
    if (first_class == second_class) {
      throw std::logic_error(
          "copy-coalescing collapsed an original interference edge");
    }
    if (second_class < first_class) {
      std::swap(first_class, second_class);
    }
    quotient_edges.emplace(first_class, second_class);
  }

  std::vector<std::vector<std::size_t>> adjacency(result.classes.size());
  result.quotient_interference_edges.reserve(quotient_edges.size());
  for (const auto& [first, second] : quotient_edges) {
    result.quotient_interference_edges.push_back(
        QuotientInterferenceEdge{first, second});
    adjacency[first].push_back(second);
    adjacency[second].push_back(first);
  }

  std::vector<std::size_t> order(result.classes.size());
  std::iota(order.begin(), order.end(), 0U);
  std::sort(order.begin(), order.end(),
            [&adjacency](const std::size_t first,
                         const std::size_t second) {
              if (adjacency[first].size() != adjacency[second].size()) {
                return adjacency[first].size() > adjacency[second].size();
              }
              return first < second;
            });

  std::vector<std::optional<std::size_t>> assigned(result.classes.size());
  for (const std::size_t class_id : order) {
    std::set<std::size_t> unavailable;
    for (const std::size_t neighbor : adjacency[class_id]) {
      if (assigned[neighbor].has_value()) {
        unavailable.insert(*assigned[neighbor]);
      }
    }
    std::size_t candidate = 0U;
    while (candidate < register_budget && unavailable.contains(candidate)) {
      ++candidate;
    }
    if (candidate < register_budget) {
      assigned[class_id] = candidate;
    }
  }

  for (std::size_t class_id = 0U; class_id < result.classes.size(); ++class_id) {
    result.classes[class_id].physical_register = assigned[class_id];
  }

  result.assignments.reserve(location_count);
  for (std::size_t location = 0U; location < location_count; ++location) {
    const std::optional<std::size_t> physical = assigned[location_class[location]];
    result.assignments.push_back(
        PhysicalRegisterAssignment{result.locations[location], physical});
    if (!physical.has_value()) {
      result.spills.push_back(result.locations[location]);
    }
  }

  for (const MoveReference& reference : moves) {
    const std::size_t source =
        location_index(result.locations, reference.move.source);
    const std::size_t destination =
        location_index(result.locations, reference.move.destination);
    if (location_class[source] == location_class[destination]) {
      result.redundant_copies.push_back(RedundantScheduledCopy{
          reference.block, reference.kind, reference.index, reference.move});
    }
  }

  return result;
}

}  // namespace algorithms::graphs
