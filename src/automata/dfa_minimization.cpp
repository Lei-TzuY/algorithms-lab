#include "algorithms/automata/dfa_minimization.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::automata {
namespace {

using State = std::size_t;
using BlockId = std::size_t;

struct WorkItem {
  BlockId block;
  std::size_t symbol;
};

void validate_dfa(const Dfa& dfa) {
  const std::size_t state_count = dfa.transitions.size();
  if (state_count == 0U) {
    throw std::invalid_argument("DFA must contain at least one state");
  }
  if (dfa.accepting.size() != state_count) {
    throw std::invalid_argument("DFA accepting vector size mismatch");
  }
  if (dfa.start_state >= state_count) {
    throw std::invalid_argument("DFA start state is out of range");
  }

  const std::size_t alphabet_size = dfa.transitions.front().size();
  for (const auto& row : dfa.transitions) {
    if (row.size() != alphabet_size) {
      throw std::invalid_argument("DFA transition rows must have equal size");
    }
    for (const State target : row) {
      if (target >= state_count) {
        throw std::invalid_argument("DFA transition target is out of range");
      }
    }
  }
}

std::vector<unsigned char> reachable_states(const Dfa& dfa) {
  const std::size_t state_count = dfa.transitions.size();
  std::vector<unsigned char> reachable(state_count, 0U);
  std::vector<State> queue;
  queue.reserve(state_count);
  reachable[dfa.start_state] = 1U;
  queue.push_back(dfa.start_state);

  for (std::size_t head = 0U; head < queue.size(); ++head) {
    const State state = queue[head];
    for (const State next : dfa.transitions[state]) {
      if (reachable[next] == 0U) {
        reachable[next] = 1U;
        queue.push_back(next);
      }
    }
  }
  return reachable;
}

}  // namespace

MinimizedDfa minimize_dfa(const Dfa& dfa) {
  validate_dfa(dfa);

  const std::size_t state_count = dfa.transitions.size();
  const std::size_t alphabet_size = dfa.transitions.front().size();
  const auto reachable = reachable_states(dfa);

  std::vector<State> rejecting;
  std::vector<State> accepting;
  rejecting.reserve(state_count);
  accepting.reserve(state_count);
  for (State state = 0U; state < state_count; ++state) {
    if (reachable[state] == 0U) {
      continue;
    }
    (dfa.accepting[state] ? accepting : rejecting).push_back(state);
  }

  std::vector<std::vector<State>> blocks;
  if (!rejecting.empty()) {
    blocks.push_back(std::move(rejecting));
  }
  if (!accepting.empty()) {
    blocks.push_back(std::move(accepting));
  }

  std::vector<BlockId> block_of(state_count, 0U);
  for (BlockId block = 0U; block < blocks.size(); ++block) {
    for (const State state : blocks[block]) {
      block_of[state] = block;
    }
  }

  std::vector<std::vector<std::vector<State>>> predecessors(
      alphabet_size, std::vector<std::vector<State>>(state_count));
  for (State state = 0U; state < state_count; ++state) {
    if (reachable[state] == 0U) {
      continue;
    }
    for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
      predecessors[symbol][dfa.transitions[state][symbol]].push_back(state);
    }
  }

  std::deque<WorkItem> work;
  std::vector<std::vector<unsigned char>> queued(
      blocks.size(), std::vector<unsigned char>(alphabet_size, 0U));
  auto enqueue = [&](BlockId block, std::size_t symbol) {
    if (queued[block][symbol] == 0U) {
      queued[block][symbol] = 1U;
      work.push_back(WorkItem{block, symbol});
    }
  };

  if (alphabet_size != 0U) {
    BlockId initial = 0U;
    if (blocks.size() == 2U && blocks[1U].size() < blocks[0U].size()) {
      initial = 1U;
    }
    for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
      enqueue(initial, symbol);
    }
  }

  std::vector<std::vector<State>> affected_states(blocks.size());
  std::vector<BlockId> touched_blocks;
  std::vector<unsigned char> marked(state_count, 0U);

  while (!work.empty()) {
    const WorkItem item = work.front();
    work.pop_front();
    queued[item.block][item.symbol] = 0U;

    touched_blocks.clear();
    for (const State target : blocks[item.block]) {
      for (const State predecessor : predecessors[item.symbol][target]) {
        const BlockId owner = block_of[predecessor];
        if (affected_states[owner].empty()) {
          touched_blocks.push_back(owner);
        }
        affected_states[owner].push_back(predecessor);
      }
    }

    for (const BlockId owner : touched_blocks) {
      auto& inside = affected_states[owner];
      if (inside.size() == blocks[owner].size()) {
        inside.clear();
        continue;
      }

      for (const State state : inside) {
        marked[state] = 1U;
      }

      std::vector<State> outside;
      outside.reserve(blocks[owner].size() - inside.size());
      for (const State state : blocks[owner]) {
        if (marked[state] == 0U) {
          outside.push_back(state);
        }
      }
      for (const State state : inside) {
        marked[state] = 0U;
      }

      std::vector<State> new_block = std::move(inside);
      inside.clear();
      blocks[owner] = std::move(outside);
      const BlockId new_id = blocks.size();
      blocks.push_back(std::move(new_block));
      affected_states.emplace_back();
      queued.emplace_back(alphabet_size, 0U);

      for (const State state : blocks[new_id]) {
        block_of[state] = new_id;
      }

      for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
        if (queued[owner][symbol] != 0U) {
          enqueue(new_id, symbol);
        } else if (blocks[new_id].size() < blocks[owner].size()) {
          enqueue(new_id, symbol);
        } else {
          enqueue(owner, symbol);
        }
      }
    }
  }

  for (auto& block : blocks) {
    std::sort(block.begin(), block.end());
  }

  const BlockId raw_start = block_of[dfa.start_state];
  std::vector<std::optional<std::size_t>> canonical(blocks.size(), std::nullopt);
  std::vector<BlockId> bfs_blocks;
  bfs_blocks.reserve(blocks.size());
  canonical[raw_start] = 0U;
  bfs_blocks.push_back(raw_start);

  for (std::size_t head = 0U; head < bfs_blocks.size(); ++head) {
    const BlockId block = bfs_blocks[head];
    const State representative = blocks[block].front();
    for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
      const BlockId next = block_of[dfa.transitions[representative][symbol]];
      if (!canonical[next].has_value()) {
        canonical[next] = bfs_blocks.size();
        bfs_blocks.push_back(next);
      }
    }
  }

  MinimizedDfa result;
  result.start_state = 0U;
  result.transitions.resize(blocks.size(),
                            std::vector<std::size_t>(alphabet_size, 0U));
  result.accepting.resize(blocks.size(), false);
  result.original_to_minimized.resize(state_count, std::nullopt);
  result.minimized_to_original.resize(blocks.size());

  for (std::size_t canonical_id = 0U; canonical_id < bfs_blocks.size();
       ++canonical_id) {
    const BlockId raw = bfs_blocks[canonical_id];
    const State representative = blocks[raw].front();
    result.accepting[canonical_id] = dfa.accepting[representative];
    result.minimized_to_original[canonical_id] = blocks[raw];
    for (const State state : blocks[raw]) {
      result.original_to_minimized[state] = canonical_id;
    }
    for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
      const BlockId next_raw = block_of[dfa.transitions[representative][symbol]];
      result.transitions[canonical_id][symbol] = canonical[next_raw].value();
    }
  }

  return result;
}

}  // namespace algorithms::automata
