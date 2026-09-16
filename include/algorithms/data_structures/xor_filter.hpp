#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class Xor8Filter64 {
 public:
  static Xor8Filter64 build(std::vector<std::uint64_t> keys,
                            std::uint64_t seed = 0,
                            std::size_t max_attempts = 256U) {
    if (max_attempts == 0U) {
      throw std::invalid_argument("Xor8Filter64 max_attempts must be positive");
    }

    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    Xor8Filter64 result;
    result.key_count_ = keys.size();
    result.requested_seed_ = seed;
    if (keys.empty()) {
      result.seed_ = seed;
      return result;
    }

    const std::size_t quotient = keys.size() / 3U;
    const std::size_t remainder = keys.size() % 3U;
    if (quotient > std::numeric_limits<std::size_t>::max() / 2U) {
      throw std::length_error("Xor8Filter64 block size overflows size_t");
    }
    const std::size_t block_size = quotient * 2U + remainder;
    if (block_size > std::numeric_limits<std::size_t>::max() / 3U) {
      throw std::length_error("Xor8Filter64 slot count overflows size_t");
    }
    const std::size_t slot_count = block_size * 3U;

    for (std::size_t attempt = 0; attempt < max_attempts; ++attempt) {
      const std::uint64_t trial_seed =
          seed + static_cast<std::uint64_t>(attempt) * UINT64_C(0x9e3779b97f4a7c15);
      std::vector<std::uint8_t> fingerprints;
      if (try_build(keys, block_size, trial_seed, fingerprints)) {
        result.block_size_ = block_size;
        result.seed_ = trial_seed;
        result.attempts_ = attempt + 1U;
        result.fingerprints_ = std::move(fingerprints);
        if (result.fingerprints_.size() != slot_count) {
          throw std::logic_error("Xor8Filter64 internal slot-count mismatch");
        }
        for (const std::uint64_t key : keys) {
          if (!result.contains(key)) {
            throw std::logic_error("Xor8Filter64 construction lost an inserted key");
          }
        }
        return result;
      }
    }

    throw std::runtime_error("Xor8Filter64 could not peel the construction hypergraph");
  }

  [[nodiscard]] bool contains(std::uint64_t key) const noexcept {
    if (block_size_ == 0U) {
      return false;
    }
    const auto positions = locations(key, block_size_, seed_);
    const std::uint8_t observed = static_cast<std::uint8_t>(
        fingerprints_[positions[0]] ^ fingerprints_[positions[1]] ^
        fingerprints_[positions[2]]);
    return observed == fingerprint(key, seed_);
  }

  [[nodiscard]] bool empty() const noexcept { return key_count_ == 0U; }
  [[nodiscard]] std::size_t size() const noexcept { return key_count_; }
  [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
  [[nodiscard]] std::size_t slot_count() const noexcept { return fingerprints_.size(); }
  [[nodiscard]] std::uint64_t requested_seed() const noexcept { return requested_seed_; }
  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] std::size_t construction_attempts() const noexcept { return attempts_; }
  [[nodiscard]] const std::vector<std::uint8_t>& debug_fingerprints() const noexcept {
    return fingerprints_;
  }

 private:
  struct Edge {
    std::array<std::size_t, 3> vertices{};
    std::uint8_t fingerprint{0};
  };

  struct PeelStep {
    std::size_t edge{0};
    std::size_t vertex{0};
  };

  static bool try_build(const std::vector<std::uint64_t>& keys,
                        std::size_t block_size, std::uint64_t seed,
                        std::vector<std::uint8_t>& output) {
    const std::size_t slot_count = block_size * 3U;
    std::vector<Edge> edges;
    edges.reserve(keys.size());
    std::vector<std::size_t> degree(slot_count, 0U);
    std::vector<std::size_t> incident_xor(slot_count, 0U);

    for (const std::uint64_t key : keys) {
      Edge edge{locations(key, block_size, seed), fingerprint(key, seed)};
      const std::size_t edge_index = edges.size();
      edges.push_back(edge);
      for (const std::size_t vertex : edge.vertices) {
        ++degree[vertex];
        incident_xor[vertex] ^= edge_index;
      }
    }

    std::vector<std::size_t> queue;
    queue.reserve(slot_count);
    for (std::size_t vertex = 0; vertex < slot_count; ++vertex) {
      if (degree[vertex] == 1U) {
        queue.push_back(vertex);
      }
    }

    std::vector<bool> removed(edges.size(), false);
    std::vector<PeelStep> peel;
    peel.reserve(edges.size());
    std::size_t cursor = 0U;
    while (cursor < queue.size()) {
      const std::size_t vertex = queue[cursor++];
      if (degree[vertex] != 1U) {
        continue;
      }
      const std::size_t edge_index = incident_xor[vertex];
      if (edge_index >= edges.size() || removed[edge_index]) {
        return false;
      }
      removed[edge_index] = true;
      peel.push_back(PeelStep{edge_index, vertex});
      for (const std::size_t adjacent : edges[edge_index].vertices) {
        if (degree[adjacent] == 0U) {
          return false;
        }
        --degree[adjacent];
        incident_xor[adjacent] ^= edge_index;
        if (degree[adjacent] == 1U) {
          queue.push_back(adjacent);
        }
      }
    }

    if (peel.size() != edges.size()) {
      return false;
    }

    output.assign(slot_count, std::uint8_t{0});
    for (auto it = peel.rbegin(); it != peel.rend(); ++it) {
      const Edge& edge = edges[it->edge];
      std::uint8_t value = edge.fingerprint;
      for (const std::size_t vertex : edge.vertices) {
        if (vertex != it->vertex) {
          value = static_cast<std::uint8_t>(value ^ output[vertex]);
        }
      }
      output[it->vertex] = value;
    }
    return true;
  }

  static std::array<std::size_t, 3> locations(std::uint64_t key,
                                               std::size_t block_size,
                                               std::uint64_t seed) noexcept {
    const std::uint64_t first = mix(key ^ (seed + UINT64_C(0x243f6a8885a308d3)));
    const std::uint64_t second = mix(key ^ (seed + UINT64_C(0x13198a2e03707344)));
    const std::uint64_t third = mix(key ^ (seed + UINT64_C(0xa4093822299f31d0)));
    const auto block = static_cast<std::uint64_t>(block_size);
    return {
        static_cast<std::size_t>(first % block),
        block_size + static_cast<std::size_t>(second % block),
        block_size * 2U + static_cast<std::size_t>(third % block),
    };
  }

  static std::uint8_t fingerprint(std::uint64_t key, std::uint64_t seed) noexcept {
    return static_cast<std::uint8_t>(
        mix(key ^ (seed + UINT64_C(0x082efa98ec4e6c89))) & UINT64_C(0xff));
  }

  static std::uint64_t mix(std::uint64_t value) noexcept {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
  }

  std::size_t block_size_{0};
  std::size_t key_count_{0};
  std::uint64_t requested_seed_{0};
  std::uint64_t seed_{0};
  std::size_t attempts_{0};
  std::vector<std::uint8_t> fingerprints_;
};

}  // namespace algorithms::data_structures
