#pragma once

#include "algorithms/data_structures/persistent_byte_rope.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace {

using algorithms::data_structures::PersistentByteRope;

std::string persistent_rope_random_bytes(std::mt19937_64& rng,
                                         std::size_t length) {
  std::string result(length, '\0');
  for (char& value : result) {
    value = static_cast<char>(static_cast<unsigned char>(rng() & 0xffU));
  }
  return result;
}

void require_persistent_rope_matches(const PersistentByteRope& rope,
                                     const std::string& expected,
                                     std::mt19937_64& rng) {
  REQUIRE(rope.valid_structure());
  REQUIRE_EQ(rope.size(), expected.size());
  REQUIRE_EQ(rope.to_string(), expected);
  if (expected.empty()) {
    REQUIRE(rope.empty());
    REQUIRE_EQ(rope.height(), std::size_t{0});
    REQUIRE_EQ(rope.leaf_count(), std::size_t{0});
    return;
  }
  REQUIRE(!rope.empty());
  REQUIRE(rope.height() >= 1);
  REQUIRE(rope.leaf_count() >= 1);
  for (int probe = 0; probe < 5; ++probe) {
    const std::size_t index =
        static_cast<std::size_t>(rng() % expected.size());
    REQUIRE_EQ(rope.at(index),
               static_cast<std::uint8_t>(
                   static_cast<unsigned char>(expected[index])));
  }
}

TEST_CASE(persistent_byte_rope_boundaries_and_arbitrary_bytes) {
  PersistentByteRope empty;
  REQUIRE(empty.empty());
  REQUIRE(empty.valid_structure());
  REQUIRE_EQ(empty.to_string(), std::string{});
  REQUIRE_THROWS_AS(empty.at(0), std::out_of_range);
  REQUIRE_THROWS_AS(empty.split(1), std::out_of_range);
  REQUIRE_THROWS_AS(empty.insert(1, "x"), std::out_of_range);
  REQUIRE_THROWS_AS(empty.erase(0, 1), std::out_of_range);
  REQUIRE_THROWS_AS(empty.slice(1, 0), std::out_of_range);

  std::string bytes;
  bytes.push_back('\0');
  bytes.push_back(static_cast<char>(0x80));
  bytes.push_back(static_cast<char>(0xff));
  bytes += "abc";
  const PersistentByteRope rope(bytes);
  REQUIRE(rope.valid_structure());
  REQUIRE_EQ(rope.to_string(), bytes);
  REQUIRE_EQ(rope.at(0), std::uint8_t{0});
  REQUIRE_EQ(rope.at(1), std::uint8_t{0x80});
  REQUIRE_EQ(rope.at(2), std::uint8_t{0xff});
  REQUIRE_THROWS_AS(rope.at(bytes.size()), std::out_of_range);

  const auto [all_left, no_right] = rope.split(bytes.size());
  REQUIRE_EQ(all_left.to_string(), bytes);
  REQUIRE(no_right.empty());
  const auto [no_left, all_right] = rope.split(0);
  REQUIRE(no_left.empty());
  REQUIRE_EQ(all_right.to_string(), bytes);
}

TEST_CASE(persistent_byte_rope_split_concat_and_structural_sharing) {
  std::string text(4096, '\0');
  for (std::size_t index = 0; index < text.size(); ++index) {
    text[index] =
        static_cast<char>(static_cast<unsigned char>(index & 0xffU));
  }
  const PersistentByteRope original(text);
  REQUIRE(original.valid_structure());
  REQUIRE_EQ(original.leaf_count(), std::size_t{64});

  const auto [left, right] = original.split(2048);
  REQUIRE_EQ(left.to_string(), text.substr(0, 2048));
  REQUIRE_EQ(right.to_string(), text.substr(2048));
  REQUIRE(left.shared_node_count_with(original) > 0);
  REQUIRE(right.shared_node_count_with(original) > 0);

  const PersistentByteRope joined = PersistentByteRope::concat(left, right);
  REQUIRE(joined.valid_structure());
  REQUIRE_EQ(joined.to_string(), text);

  const PersistentByteRope doubled =
      PersistentByteRope::concat(original, original);
  REQUIRE(doubled.valid_structure());
  REQUIRE_EQ(doubled.to_string(), text + text);
  REQUIRE(doubled.shared_node_count_with(original) > 0);
  REQUIRE(doubled.unique_node_count() < original.unique_node_count() * 2 + 1);

  const PersistentByteRope inserted = original.insert(2000, "persistent");
  REQUIRE(inserted.valid_structure());
  REQUIRE(inserted.shared_node_count_with(original) > 0);
  REQUIRE_EQ(original.to_string(), text);
  std::string expected_inserted = text;
  expected_inserted.insert(2000, "persistent");
  REQUIRE_EQ(inserted.to_string(), expected_inserted);

  const PersistentByteRope erased = inserted.erase(1990, 2030);
  std::string expected_erased = expected_inserted;
  expected_erased.erase(1990, 40);
  REQUIRE_EQ(erased.to_string(), expected_erased);
  REQUIRE_EQ(original.to_string(), text);

  const PersistentByteRope middle = original.slice(1000, 3000);
  REQUIRE_EQ(middle.to_string(), text.substr(1000, 2000));
  REQUIRE(middle.shared_node_count_with(original) > 0);
}

TEST_CASE(persistent_byte_rope_long_rebalancing_trace) {
  PersistentByteRope rope;
  std::string expected;
  for (std::size_t index = 0; index < 6000; ++index) {
    const char value =
        static_cast<char>(static_cast<unsigned char>(index & 0xffU));
    const std::string one(1, value);
    rope = rope.insert(rope.size(), one);
    expected.push_back(value);
    if ((index % 127U) == 0U) REQUIRE(rope.valid_structure());
  }
  REQUIRE(rope.valid_structure());
  REQUIRE_EQ(rope.to_string(), expected);
  REQUIRE(rope.height() < 32);

  for (std::size_t round = 0; round < 1200; ++round) {
    const std::size_t begin = rope.size() / 3;
    rope = rope.erase(begin, begin + 1);
    expected.erase(begin, 1);
    if ((round % 61U) == 0U) REQUIRE(rope.valid_structure());
  }
  REQUIRE(rope.valid_structure());
  REQUIRE_EQ(rope.to_string(), expected);
}

TEST_CASE(persistent_byte_rope_randomized_persistent_differential) {
  std::mt19937_64 rng(0xBADC0FFEE0DDF00DULL);
  std::vector<PersistentByteRope> ropes(1);
  std::vector<std::string> oracle(1);
  ropes.reserve(5001);
  oracle.reserve(5001);

  for (std::size_t step = 0; step < 5000; ++step) {
    const std::size_t base =
        static_cast<std::size_t>(rng() % ropes.size());
    const PersistentByteRope& source = ropes[base];
    const std::string& expected_source = oracle[base];
    const std::uint64_t op = rng() % 5U;

    PersistentByteRope next;
    std::string expected;
    if (op == 0U) {
      const std::size_t position =
          expected_source.empty()
              ? 0
              : static_cast<std::size_t>(rng() % (expected_source.size() + 1));
      const std::string payload = persistent_rope_random_bytes(
          rng, static_cast<std::size_t>(rng() % 33U));
      next = source.insert(position, payload);
      expected = expected_source;
      expected.insert(position, payload);
    } else if (op == 1U) {
      const std::size_t first =
          expected_source.empty()
              ? 0
              : static_cast<std::size_t>(rng() % (expected_source.size() + 1));
      const std::size_t second =
          expected_source.empty()
              ? 0
              : static_cast<std::size_t>(rng() % (expected_source.size() + 1));
      const std::size_t begin = std::min(first, second);
      const std::size_t end = std::max(first, second);
      next = source.erase(begin, end);
      expected = expected_source;
      expected.erase(begin, end - begin);
    } else if (op == 2U) {
      const std::size_t first =
          expected_source.empty()
              ? 0
              : static_cast<std::size_t>(rng() % (expected_source.size() + 1));
      const std::size_t second =
          expected_source.empty()
              ? 0
              : static_cast<std::size_t>(rng() % (expected_source.size() + 1));
      const std::size_t begin = std::min(first, second);
      const std::size_t end = std::max(first, second);
      next = source.slice(begin, end);
      expected = expected_source.substr(begin, end - begin);
    } else if (op == 3U) {
      const std::size_t other =
          static_cast<std::size_t>(rng() % ropes.size());
      if (expected_source.size() + oracle[other].size() <= 2048) {
        next = PersistentByteRope::concat(source, ropes[other]);
        expected = expected_source + oracle[other];
      } else {
        next = source;
        expected = expected_source;
      }
    } else {
      const std::size_t position =
          expected_source.empty()
              ? 0
              : static_cast<std::size_t>(rng() % (expected_source.size() + 1));
      auto [left, right] = source.split(position);
      next = PersistentByteRope::concat(left, right);
      expected = expected_source;
    }

    require_persistent_rope_matches(next, expected, rng);
    ropes.push_back(std::move(next));
    oracle.push_back(std::move(expected));

    if ((step % 37U) == 0U) {
      const std::size_t old =
          static_cast<std::size_t>(rng() % ropes.size());
      require_persistent_rope_matches(ropes[old], oracle[old], rng);
    }
  }
}

}  // namespace
