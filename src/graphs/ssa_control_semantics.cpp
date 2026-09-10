#include "algorithms/graphs/ssa_control_semantics.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

[[nodiscard]] std::vector<unsigned char> reachable_from(const Graph& graph,
                                                        const Vertex start) {
  graph.validate_vertex(start);
  std::vector<unsigned char> reachable(graph.vertex_count(), 0U);
  std::queue<Vertex> pending;
  reachable[start] = 1U;
  pending.push(start);
  while (!pending.empty()) {
    const Vertex block = pending.front();
    pending.pop();
    for (const Edge& edge : graph.neighbors(block)) {
      if (reachable[edge.to] == 0U) {
        reachable[edge.to] = 1U;
        pending.push(edge.to);
      }
    }
  }
  return reachable;
}

[[nodiscard]] bool has_edge(const Graph& graph, const Vertex from,
                            const Vertex to) {
  graph.validate_vertex(from);
  graph.validate_vertex(to);
  const auto& neighbors = graph.neighbors(from);
  return std::any_of(neighbors.begin(), neighbors.end(),
                     [to](const Edge& edge) { return edge.to == to; });
}

void require_no_payload(const SsaControlTerminatorInput& control) {
  if (control.predicate.has_value() || control.jump_successor.has_value() ||
      control.nonzero_successor.has_value() || control.zero_successor.has_value()) {
    throw std::invalid_argument("opaque control terminator carries semantic payload");
  }
}

void validate_control(const Graph& graph, const Vertex block,
                      const bool reachable, const std::size_t variable_count,
                      const SsaControlTerminatorInput& control) {
  if (!reachable) {
    if (control.kind != SsaControlTerminatorKind::opaque) {
      throw std::invalid_argument(
          "unreachable block cannot carry executable control semantics");
    }
    require_no_payload(control);
    return;
  }

  switch (control.kind) {
    case SsaControlTerminatorKind::opaque:
      require_no_payload(control);
      return;
    case SsaControlTerminatorKind::jump:
      if (control.predicate.has_value() || !control.jump_successor.has_value() ||
          control.nonzero_successor.has_value() ||
          control.zero_successor.has_value()) {
        throw std::invalid_argument("jump control terminator has malformed shape");
      }
      if (!has_edge(graph, block, *control.jump_successor)) {
        throw std::invalid_argument("jump control target is not a CFG edge");
      }
      return;
    case SsaControlTerminatorKind::branch_if_nonzero:
      if (!control.predicate.has_value() || control.jump_successor.has_value() ||
          !control.nonzero_successor.has_value() ||
          !control.zero_successor.has_value()) {
        throw std::invalid_argument(
            "conditional control terminator has malformed shape");
      }
      if (*control.predicate >= variable_count) {
        throw std::out_of_range("conditional control predicate variable out of range");
      }
      if (!has_edge(graph, block, *control.nonzero_successor) ||
          !has_edge(graph, block, *control.zero_successor)) {
        throw std::invalid_argument(
            "conditional control target is not a CFG edge");
      }
      return;
  }
  throw std::invalid_argument("unknown control terminator kind");
}

[[nodiscard]] SsaLoweredControlTarget lower_target(
    const OutOfSsaProgram& program, const Vertex predecessor,
    const Vertex logical_successor) {
  std::optional<Vertex> execution_successor;
  for (const SsaEdgeLowering& edge : program.edge_lowerings) {
    if (edge.predecessor != predecessor ||
        edge.successor != logical_successor ||
        edge.placement != SsaCopyPlacement::split_blocks) {
      continue;
    }
    if (edge.execution_blocks.empty()) {
      throw std::logic_error("split control edge has no execution block");
    }
    execution_successor = *std::min_element(edge.execution_blocks.begin(),
                                            edge.execution_blocks.end());
    break;
  }
  if (!execution_successor.has_value()) {
    execution_successor = logical_successor;
  }

  const auto& neighbors = program.graph.neighbors(predecessor);
  const bool present = std::any_of(
      neighbors.begin(), neighbors.end(), [execution_successor](const Edge& edge) {
        return edge.to == *execution_successor;
      });
  if (!present) {
    throw std::logic_error(
        "lowered control target is not an execution CFG edge");
  }
  return {logical_successor, *execution_successor};
}

}  // namespace

OutOfSsaProgram construct_control_semantic_out_of_ssa(
    const Graph& graph, const Vertex start, const std::size_t variable_count,
    const std::vector<std::vector<SemanticSsaInputInstruction>>& blocks,
    const std::vector<SsaControlTerminatorInput>& controls) {
  if (!graph.directed()) {
    throw std::invalid_argument("control semantic SSA requires a directed CFG");
  }
  if (blocks.size() != graph.vertex_count() ||
      controls.size() != graph.vertex_count()) {
    throw std::invalid_argument(
        "control semantic SSA block/control count does not match CFG");
  }

  const std::vector<unsigned char> reachable = reachable_from(graph, start);
  std::vector<std::vector<SemanticSsaInputInstruction>> augmented = blocks;
  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    validate_control(graph, block, reachable[block] != 0U, variable_count,
                     controls[block]);
    if (controls[block].kind != SsaControlTerminatorKind::branch_if_nonzero) {
      continue;
    }
    augmented[block].push_back(SemanticSsaInputInstruction{
        {*controls[block].predicate}, std::nullopt, SsaInstructionSemantics{}});
  }

  SsaProgram ssa =
      construct_semantic_ssa(graph, start, variable_count, augmented);
  std::vector<std::optional<SsaValue>> predicates(graph.vertex_count());
  for (Vertex block = 0U; block < graph.vertex_count(); ++block) {
    if (controls[block].kind != SsaControlTerminatorKind::branch_if_nonzero) {
      continue;
    }
    auto& instructions = ssa.blocks[block].instructions;
    if (instructions.empty()) {
      throw std::logic_error("conditional control marker vanished during SSA rename");
    }
    const SsaInstruction& marker = instructions.back();
    if (marker.uses.size() != 1U || marker.definition.has_value() ||
        marker.semantics != SsaInstructionSemantics{}) {
      throw std::logic_error("conditional control marker changed semantic shape");
    }
    predicates[block] = marker.uses.front();
    instructions.pop_back();
  }

  OutOfSsaProgram result = destroy_ssa(graph, ssa);
  const std::size_t original_count = graph.vertex_count();
  if (result.blocks.size() != result.graph.vertex_count() ||
      result.blocks.size() < original_count) {
    throw std::logic_error("control semantic SSA destruction changed block domain");
  }

  for (Vertex block = 0U; block < original_count; ++block) {
    SsaLoweredControlTerminator lowered;
    lowered.kind = controls[block].kind;
    switch (controls[block].kind) {
      case SsaControlTerminatorKind::opaque:
        break;
      case SsaControlTerminatorKind::jump:
        lowered.jump_target =
            lower_target(result, block, *controls[block].jump_successor);
        break;
      case SsaControlTerminatorKind::branch_if_nonzero:
        if (!predicates[block].has_value()) {
          throw std::logic_error("conditional control lost renamed predicate");
        }
        lowered.predicate = SsaCopyLocation::from_value(*predicates[block]);
        lowered.nonzero_target =
            lower_target(result, block, *controls[block].nonzero_successor);
        lowered.zero_target =
            lower_target(result, block, *controls[block].zero_successor);
        break;
    }
    result.blocks[block].control = std::move(lowered);
  }

  for (Vertex block = original_count; block < result.blocks.size(); ++block) {
    const auto& neighbors = result.graph.neighbors(block);
    if (neighbors.size() != 1U) {
      throw std::logic_error(
          "SSA split block must have exactly one execution successor");
    }
    SsaLoweredControlTerminator lowered;
    lowered.kind = SsaControlTerminatorKind::jump;
    lowered.jump_target =
        SsaLoweredControlTarget{neighbors.front().to, neighbors.front().to};
    result.blocks[block].control = std::move(lowered);
  }

  return result;
}

}  // namespace algorithms::graphs
