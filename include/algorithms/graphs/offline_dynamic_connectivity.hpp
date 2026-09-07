#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace algorithms::graphs {

enum class DynamicConnectivityOperationKind {
  AddEdge,
  RemoveEdge,
  QueryConnected,
};

struct DynamicConnectivityOperation {
  DynamicConnectivityOperationKind kind;
  std::size_t first;
  std::size_t second;
};

struct OfflineDynamicConnectivityResult {
  // Answers appear in the same order as QueryConnected operations.
  std::vector<bool> query_answers;
};

// Answers undirected connectivity queries over an offline add/remove timeline.
//
// Edge multiplicity is preserved: each AddEdge opens one active copy and each
// RemoveEdge closes one currently active copy of the same canonical undirected
// edge. Removing an inactive edge is rejected. Self-loops are legal and obey
// the same multiplicity rules, although they never change connectivity.
[[nodiscard]] OfflineDynamicConnectivityResult offline_dynamic_connectivity(
    std::size_t vertex_count,
    std::span<const DynamicConnectivityOperation> operations);

}  // namespace algorithms::graphs
