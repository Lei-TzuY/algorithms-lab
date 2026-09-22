#pragma once

#include "algorithms/coding/sparse_xor_fountain.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::coding::SparseXorFountainPacket;
using algorithms::coding::decode_sparse_xor_fountain_peeling;
using algorithms::coding::encode_sparse_xor_fountain_packet;
using algorithms::coding::make_deterministic_sparse_xor_fountain_packet;

namespace sparse_xor_fountain_test_detail {

inline std::uint64_t direct_payload(
    const std::vector<std::uint64_t>& source,
    const std::vector<std::size_t>& indices) {
  std::uint64_t value = 0U;
  for (const std::size_t index : indices) {
    value ^= source[index];
  }
  return value;
}

inline void require_packet_matches_source(
    const SparseXorFountainPacket& packet,
    const std::vector<std::uint64_t>& source) {
  REQUIRE(!packet.source_indices.empty());
  REQUIRE(std::is_sorted(
      packet.source_indices.begin(),
      packet.source_indices.end()));
  REQUIRE(std::adjacent_find(
              packet.source_indices.begin(),
              packet.source_indices.end()) ==
          packet.source_indices.end());
  for (const std::size_t index : packet.source_indices) {
    REQUIRE(index < source.size());
  }
  REQUIRE_EQ(
      packet.payload,
      direct_payload(source, packet.source_indices));
}

}  // namespace sparse_xor_fountain_test_detail

TEST_CASE(sparse_xor_fountain_explicit_packet_is_canonical) {
  using namespace sparse_xor_fountain_test_detail;

  const std::vector<std::uint64_t> source{
      UINT64_C(0x0123456789abcdef),
      UINT64_C(0xfedcba9876543210),
      UINT64_C(0x1111111111111111),
      UINT64_C(0x8000000000000000)};

  const std::vector<std::size_t> requested{3U, 0U, 2U};
  const auto packet =
      encode_sparse_xor_fountain_packet(source, requested);

  REQUIRE(packet.source_indices ==
          std::vector<std::size_t>({0U, 2U, 3U}));
  require_packet_matches_source(packet, source);
}

TEST_CASE(sparse_xor_fountain_rejects_invalid_packet_requests) {
  const std::vector<std::uint64_t> source{10U, 20U, 30U};

  REQUIRE_THROWS_AS(
      encode_sparse_xor_fountain_packet(
          source, std::vector<std::size_t>{}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      encode_sparse_xor_fountain_packet(
          source, std::vector<std::size_t>{1U, 1U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      encode_sparse_xor_fountain_packet(
          source, std::vector<std::size_t>{0U, 3U}),
      std::invalid_argument);

  REQUIRE_THROWS_AS(
      make_deterministic_sparse_xor_fountain_packet(
          std::vector<std::uint64_t>{}, 0U, 1U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      make_deterministic_sparse_xor_fountain_packet(
          source, 0U, 0U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      make_deterministic_sparse_xor_fountain_packet(
          source, 0U, 4U),
      std::invalid_argument);
}

TEST_CASE(sparse_xor_fountain_deterministic_selector_is_reproducible) {
  using namespace sparse_xor_fountain_test_detail;

  std::vector<std::uint64_t> source(31U);
  for (std::size_t index = 0U; index < source.size(); ++index) {
    source[index] =
        UINT64_C(0x9e3779b97f4a7c15) *
        static_cast<std::uint64_t>(index + 1U);
  }

  for (std::size_t degree = 1U; degree <= source.size(); ++degree) {
    const auto first =
        make_deterministic_sparse_xor_fountain_packet(
            source, 17U, degree, UINT64_C(0xabcddcba12344321));
    const auto second =
        make_deterministic_sparse_xor_fountain_packet(
            source, 17U, degree, UINT64_C(0xabcddcba12344321));

    REQUIRE(first == second);
    REQUIRE_EQ(first.source_indices.size(), degree);
    require_packet_matches_source(first, source);
  }
}

TEST_CASE(sparse_xor_fountain_peels_manual_chain) {
  const std::vector<std::uint64_t> source{
      UINT64_C(0x1111222233334444),
      UINT64_C(0x5555666677778888),
      UINT64_C(0x9999aaaabbbbcccc),
      UINT64_C(0xddddeeeeffff0000)};

  std::vector<SparseXorFountainPacket> packets;
  packets.push_back(encode_sparse_xor_fountain_packet(
      source, std::vector<std::size_t>{0U}));
  packets.push_back(encode_sparse_xor_fountain_packet(
      source, std::vector<std::size_t>{0U, 1U}));
  packets.push_back(encode_sparse_xor_fountain_packet(
      source, std::vector<std::size_t>{1U, 2U}));
  packets.push_back(encode_sparse_xor_fountain_packet(
      source, std::vector<std::size_t>{0U, 2U, 3U}));

  const auto decoded =
      decode_sparse_xor_fountain_peeling(source.size(), packets);
  REQUIRE(decoded.has_value());
  REQUIRE(*decoded == source);
}

TEST_CASE(sparse_xor_fountain_reports_stopping_set_not_gaussian_solution) {
  const std::vector<std::uint64_t> source{5U, 7U, 11U};

  // These three equations have a unique GF(2) solution, but none starts at
  // degree one. The peeling-only contract must therefore stall.
  const std::vector<SparseXorFountainPacket> packets{
      encode_sparse_xor_fountain_packet(
          source, std::vector<std::size_t>{0U, 1U}),
      encode_sparse_xor_fountain_packet(
          source, std::vector<std::size_t>{1U, 2U}),
      encode_sparse_xor_fountain_packet(
          source, std::vector<std::size_t>{0U, 1U, 2U})};

  const auto decoded =
      decode_sparse_xor_fountain_peeling(source.size(), packets);
  REQUIRE(!decoded.has_value());
}

TEST_CASE(sparse_xor_fountain_nonempty_source_without_packets_stalls) {
  const auto decoded =
      decode_sparse_xor_fountain_peeling(
          3U, std::vector<SparseXorFountainPacket>{});
  REQUIRE(!decoded.has_value());
}

TEST_CASE(sparse_xor_fountain_detects_inconsistent_equations) {
  const std::vector<SparseXorFountainPacket> packets{
      {{0U}, 41U},
      {{0U}, 42U}};

  const auto decoded =
      decode_sparse_xor_fountain_peeling(1U, packets);
  REQUIRE(!decoded.has_value());
}

TEST_CASE(sparse_xor_fountain_decode_rejects_malformed_structure) {
  REQUIRE(
      decode_sparse_xor_fountain_peeling(
          0U, std::vector<SparseXorFountainPacket>{}) ==
      std::optional<std::vector<std::uint64_t>>(
          std::vector<std::uint64_t>{}));

  REQUIRE_THROWS_AS(
      decode_sparse_xor_fountain_peeling(
          0U,
          std::vector<SparseXorFountainPacket>{
              {{0U}, 1U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      decode_sparse_xor_fountain_peeling(
          3U,
          std::vector<SparseXorFountainPacket>{
              {{}, 0U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      decode_sparse_xor_fountain_peeling(
          3U,
          std::vector<SparseXorFountainPacket>{
              {{1U, 0U}, 0U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      decode_sparse_xor_fountain_peeling(
          3U,
          std::vector<SparseXorFountainPacket>{
              {{1U, 1U}, 0U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      decode_sparse_xor_fountain_peeling(
          3U,
          std::vector<SparseXorFountainPacket>{
              {{0U, 3U}, 0U}}),
      std::invalid_argument);
}

TEST_CASE(sparse_xor_fountain_random_peelable_systems_match_source) {
  using namespace sparse_xor_fountain_test_detail;

  std::mt19937_64 random(UINT64_C(0xF017A1A5EED12345));

  for (std::size_t trial = 0U; trial < 320U; ++trial) {
    const std::size_t source_count =
        1U + static_cast<std::size_t>(random() % 40U);

    std::vector<std::uint64_t> source(source_count);
    for (std::uint64_t& value : source) {
      const std::uint64_t pick = random() % 17U;
      if (pick == 0U) {
        value = 0U;
      } else if (pick == 1U) {
        value = std::numeric_limits<std::uint64_t>::max();
      } else {
        value = random();
      }
    }

    std::vector<std::size_t> order(source_count);
    std::iota(order.begin(), order.end(), 0U);
    std::shuffle(order.begin(), order.end(), random);

    // Independent peelable-system generator:
    // packet t contains the new symbol order[t] and an arbitrary subset of
    // already-earlier symbols. Once packets 0..t-1 peel, packet t becomes
    // degree one. This construction does not reuse the production queue logic.
    std::vector<SparseXorFountainPacket> packets;
    packets.reserve(source_count * 2U);

    for (std::size_t t = 0U; t < source_count; ++t) {
      std::vector<std::size_t> indices{order[t]};
      for (std::size_t earlier = 0U; earlier < t; ++earlier) {
        if ((random() % 3U) == 0U) {
          indices.push_back(order[earlier]);
        }
      }
      packets.push_back(
          encode_sparse_xor_fountain_packet(source, indices));
    }

    // Add independent deterministic sparse packets as consistent redundancy.
    for (std::size_t extra = 0U; extra < source_count; ++extra) {
      const std::size_t degree =
          1U + static_cast<std::size_t>(
                   random() % std::min<std::size_t>(source_count, 8U));
      packets.push_back(
          make_deterministic_sparse_xor_fountain_packet(
              source, trial * 97U + extra, degree,
              UINT64_C(0xC0DEC0DEF017A1A5)));
    }

    std::shuffle(packets.begin(), packets.end(), random);

    for (const auto& packet : packets) {
      require_packet_matches_source(packet, source);
    }

    const auto decoded =
        decode_sparse_xor_fountain_peeling(
            source_count, packets);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == source);
  }
}
