#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::dynamic_programming {

struct MatrixChainSplit {
  std::size_t first_matrix{0};
  std::size_t last_matrix{0};
  std::size_t split_after{0};
};

struct MatrixChainResult {
  std::uint64_t minimum_scalar_multiplications{0};
  std::vector<MatrixChainSplit> splits;
};

// dimensions describes matrices A_i with shape dimensions[i] x
// dimensions[i + 1]. Empty and one-element dimension vectors describe an
// empty chain. Every supplied dimension must be strictly positive.
//
// The returned split records are in preorder. For a non-leaf interval [i, j],
// the next record gives the split k, followed by the left [i, k] and right
// [k + 1, j] subplans. Equal-cost choices retain the leftmost split.
MatrixChainResult matrix_chain_order(
    const std::vector<std::uint64_t>& dimensions);

}  // namespace algorithms::dynamic_programming
