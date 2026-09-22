#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::coding {

struct SparseXorFountainPacket {
  std::vector<std::size_t> source_indices;
  std::uint64_t payload{};

  friend bool operator==(const SparseXorFountainPacket&,
                         const SparseXorFountainPacket&) = default;
};

namespace sparse_xor_fountain_detail {

[[nodiscard]] inline std::uint64_t splitmix64(
    std::uint64_t value) noexcept {
  value += UINT64_C(0x9e3779b97f4a7c15);
  value = (value ^ (value >> 30U)) *
          UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27U)) *
          UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31U);
}

class DeterministicRng {
 public:
  explicit DeterministicRng(const std::uint64_t seed) noexcept
      : state_(seed) {}

  [[nodiscard]] std::uint64_t next() noexcept {
    state_ = splitmix64(state_);
    return state_;
  }

  [[nodiscard]] std::size_t bounded(
      const std::size_t bound) {
    if (bound == 0U) {
      throw std::invalid_argument(
          "sparse XOR fountain random bound must be positive");
    }

    if constexpr (
        std::numeric_limits<std::size_t>::digits >
        std::numeric_limits<std::uint64_t>::digits) {
      if (bound >
          static_cast<std::size_t>(
              std::numeric_limits<std::uint64_t>::max())) {
        throw std::length_error(
            "sparse XOR fountain bound exceeds uint64 RNG domain");
      }
    }

    const std::uint64_t bound64 =
        static_cast<std::uint64_t>(bound);
    const std::uint64_t limit =
        std::numeric_limits<std::uint64_t>::max() -
        (std::numeric_limits<std::uint64_t>::max() % bound64);

    std::uint64_t value = 0U;
    do {
      value = next();
    } while (value >= limit);

    return static_cast<std::size_t>(value % bound64);
  }

 private:
  std::uint64_t state_;
};

inline void validate_canonical_indices(
    const std::span<const std::size_t> indices,
    const std::size_t source_count) {
  if (indices.empty()) {
    throw std::invalid_argument(
        "sparse XOR fountain packet must reference a source symbol");
  }

  std::optional<std::size_t> previous;
  for (const std::size_t index : indices) {
    if (index >= source_count) {
      throw std::invalid_argument(
          "sparse XOR fountain source index out of range");
    }
    if (previous.has_value() && index <= *previous) {
      throw std::invalid_argument(
          "sparse XOR fountain indices must be strictly increasing");
    }
    previous = index;
  }
}

}  // namespace sparse_xor_fountain_detail

// Encode an explicit sparse XOR equation. The caller may provide source indices
// in any order, but duplicate or out-of-range indices are rejected. The
// returned packet is canonicalized to strictly increasing source indices.
[[nodiscard]] inline SparseXorFountainPacket
encode_sparse_xor_fountain_packet(
    const std::span<const std::uint64_t> source,
    const std::span<const std::size_t> source_indices) {
  if (source_indices.empty()) {
    throw std::invalid_argument(
        "sparse XOR fountain packet degree must be positive");
  }

  std::vector<std::size_t> indices(
      source_indices.begin(), source_indices.end());
  std::sort(indices.begin(), indices.end());

  for (std::size_t i = 0U; i < indices.size(); ++i) {
    if (indices[i] >= source.size()) {
      throw std::invalid_argument(
          "sparse XOR fountain source index out of range");
    }
    if (i != 0U && indices[i - 1U] == indices[i]) {
      throw std::invalid_argument(
          "sparse XOR fountain packet repeats a source index");
    }
  }

  std::uint64_t payload = 0U;
  for (const std::size_t index : indices) {
    payload ^= source[index];
  }

  return SparseXorFountainPacket{
      std::move(indices), payload};
}

// Deterministically select 'degree' distinct source indices from packet_id,
// degree, and seed, then encode their XOR. This is a reproducible sparse packet
// selector; it deliberately makes no robust-soliton or recovery-probability
// claim.
[[nodiscard]] inline SparseXorFountainPacket
make_deterministic_sparse_xor_fountain_packet(
    const std::span<const std::uint64_t> source,
    const std::size_t packet_id,
    const std::size_t degree,
    const std::uint64_t seed =
        UINT64_C(0x535041525345584f)) {
  if (source.empty()) {
    throw std::invalid_argument(
        "sparse XOR fountain source must be non-empty");
  }
  if (degree == 0U || degree > source.size()) {
    throw std::invalid_argument(
        "sparse XOR fountain degree out of range");
  }

  std::vector<std::size_t> pool(source.size());
  std::iota(pool.begin(), pool.end(), std::size_t{0});

  const std::uint64_t packet_key =
      sparse_xor_fountain_detail::splitmix64(
          static_cast<std::uint64_t>(packet_id));
  sparse_xor_fountain_detail::DeterministicRng random(
      seed ^ packet_key ^
      sparse_xor_fountain_detail::splitmix64(
          static_cast<std::uint64_t>(degree)));

  for (std::size_t i = 0U; i < degree; ++i) {
    const std::size_t offset =
        random.bounded(pool.size() - i);
    std::swap(pool[i], pool[i + offset]);
  }

  std::vector<std::size_t> selected(
      pool.begin(),
      pool.begin() + static_cast<std::ptrdiff_t>(degree));
  std::sort(selected.begin(), selected.end());
  return encode_sparse_xor_fountain_packet(source, selected);
}

// Exact queue-based peeling over sparse XOR equations.
//
// Returns the recovered source iff every source symbol is resolved by iterative
// degree-one peeling and all equations remain consistent. Returns nullopt for a
// stopping set, missing coverage, or inconsistent payload equations.
//
// Structural packet errors (empty equation, duplicate/unsorted indices,
// out-of-range indices) are rejected with invalid_argument.
//
// Important: nullopt does not imply that the equation system has no unique
// solution under full Gaussian elimination; peeling is intentionally the only
// solver implemented here.
[[nodiscard]] inline std::optional<std::vector<std::uint64_t>>
decode_sparse_xor_fountain_peeling(
    const std::size_t source_count,
    const std::span<const SparseXorFountainPacket> packets) {
  if (source_count == 0U) {
    if (!packets.empty()) {
      throw std::invalid_argument(
          "zero-source fountain decode requires no packets");
    }
    return std::vector<std::uint64_t>{};
  }

  struct Equation {
    std::size_t remaining{};
    std::size_t index_xor{};
    std::uint64_t payload{};
  };

  std::vector<Equation> equations;
  equations.reserve(packets.size());

  std::vector<std::vector<std::size_t>> incident(source_count);
  for (std::size_t equation_id = 0U;
       equation_id < packets.size(); ++equation_id) {
    const auto& packet = packets[equation_id];
    sparse_xor_fountain_detail::validate_canonical_indices(
        packet.source_indices, source_count);

    std::size_t index_xor = 0U;
    for (const std::size_t index : packet.source_indices) {
      index_xor ^= index;
      incident[index].push_back(equation_id);
    }

    equations.push_back(Equation{
        packet.source_indices.size(),
        index_xor,
        packet.payload});
  }

  std::vector<std::size_t> queue;
  queue.reserve(packets.size());
  for (std::size_t equation_id = 0U;
       equation_id < equations.size(); ++equation_id) {
    if (equations[equation_id].remaining == 1U) {
      queue.push_back(equation_id);
    }
  }

  std::vector<std::optional<std::uint64_t>> recovered(
      source_count);
  std::size_t recovered_count = 0U;
  std::size_t cursor = 0U;

  while (cursor < queue.size()) {
    const std::size_t equation_id = queue[cursor++];
    Equation& equation = equations[equation_id];
    if (equation.remaining != 1U) {
      continue;
    }

    const std::size_t symbol = equation.index_xor;
    if (symbol >= source_count) {
      throw std::logic_error(
          "sparse XOR fountain peeling index invariant violated");
    }
    const std::uint64_t value = equation.payload;

    if (recovered[symbol].has_value()) {
      if (*recovered[symbol] != value) {
        return std::nullopt;
      }
      continue;
    }

    recovered[symbol] = value;
    ++recovered_count;

    for (const std::size_t affected_id : incident[symbol]) {
      Equation& affected = equations[affected_id];
      if (affected.remaining == 0U) {
        continue;
      }

      --affected.remaining;
      affected.index_xor ^= symbol;
      affected.payload ^= value;

      if (affected.remaining == 0U) {
        if (affected.payload != 0U) {
          return std::nullopt;
        }
      } else if (affected.remaining == 1U) {
        queue.push_back(affected_id);
      }
    }
  }

  if (recovered_count != source_count) {
    return std::nullopt;
  }

  std::vector<std::uint64_t> output;
  output.reserve(source_count);
  for (const auto& value : recovered) {
    if (!value.has_value()) {
      throw std::logic_error(
          "sparse XOR fountain recovery count invariant violated");
    }
    output.push_back(*value);
  }
  return output;
}

}  // namespace algorithms::coding
