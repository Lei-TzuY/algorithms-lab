#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::coding {

struct ViterbiDecodeResult {
  std::vector<std::uint8_t> message_bits;
  std::vector<std::uint8_t> corrected_codeword;
  std::size_t hamming_distance{};
  std::size_t final_state{};

  friend bool operator==(const ViterbiDecodeResult&, const ViterbiDecodeResult&) = default;
};

class RateHalfConvolutionalCode {
 public:
  RateHalfConvolutionalCode(std::size_t constraint_length,
                            std::uint32_t first_generator,
                            std::uint32_t second_generator)
      : constraint_length_(constraint_length),
        first_generator_(first_generator),
        second_generator_(second_generator) {
    if (constraint_length_ < 2 || constraint_length_ > 16) {
      throw std::invalid_argument("constraint length must be in [2,16]");
    }
    const std::uint32_t live_mask =
        (std::uint32_t{1} << static_cast<unsigned>(constraint_length_)) - 1U;
    if (first_generator_ == 0U || second_generator_ == 0U ||
        (first_generator_ & ~live_mask) != 0U ||
        (second_generator_ & ~live_mask) != 0U) {
      throw std::invalid_argument("generator masks must be nonzero and fit constraint length");
    }
  }

  [[nodiscard]] std::size_t constraint_length() const noexcept {
    return constraint_length_;
  }

  [[nodiscard]] std::size_t state_count() const noexcept {
    return std::size_t{1} << static_cast<unsigned>(constraint_length_ - 1U);
  }

  [[nodiscard]] std::vector<std::uint8_t> encode(
      const std::vector<std::uint8_t>& message_bits) const {
    std::vector<std::uint8_t> result;
    if (message_bits.size() > std::numeric_limits<std::size_t>::max() / 2U) {
      throw std::length_error("encoded length is not representable");
    }
    result.reserve(message_bits.size() * 2U);

    std::size_t state = 0U;
    for (const std::uint8_t bit : message_bits) {
      validate_bit(bit);
      const Transition transition = transition_from(state, bit);
      result.push_back(transition.first_output);
      result.push_back(transition.second_output);
      state = transition.next_state;
    }
    return result;
  }

  [[nodiscard]] ViterbiDecodeResult decode_hard(
      const std::vector<std::uint8_t>& received_bits) const {
    if ((received_bits.size() % 2U) != 0U) {
      throw std::invalid_argument("received hard-decision word must have even length");
    }
    for (const std::uint8_t bit : received_bits) {
      validate_bit(bit);
    }

    const std::size_t steps = received_bits.size() / 2U;
    const std::size_t states = state_count();
    if (steps != 0U && states > std::numeric_limits<std::size_t>::max() / steps) {
      throw std::length_error("Viterbi predecessor table is not representable");
    }

    constexpr std::size_t infinity = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> current_metric(states, infinity);
    std::vector<std::size_t> next_metric(states, infinity);
    std::vector<std::size_t> current_rank(states, infinity);
    std::vector<std::size_t> next_rank(states, infinity);
    std::vector<std::size_t> selected_predecessor(states, 0U);
    std::vector<std::uint8_t> selected_input(states, 0U);
    std::vector<std::size_t> predecessor(steps * states, 0U);
    std::vector<std::uint8_t> input_bit(steps * states, 0U);
    std::vector<std::size_t> rank_slots(states * 2U, infinity);

    current_metric[0] = 0U;
    current_rank[0] = 0U;

    for (std::size_t step = 0; step < steps; ++step) {
      std::fill(next_metric.begin(), next_metric.end(), infinity);
      std::fill(next_rank.begin(), next_rank.end(), infinity);

      for (std::size_t state = 0; state < states; ++state) {
        if (current_metric[state] == infinity) {
          continue;
        }
        for (std::uint8_t bit = 0U; bit <= 1U; ++bit) {
          const Transition transition = transition_from(state, bit);
          const std::size_t mismatch =
              static_cast<std::size_t>(transition.first_output != received_bits[2U * step]) +
              static_cast<std::size_t>(transition.second_output != received_bits[2U * step + 1U]);
          if (current_metric[state] > infinity - mismatch) {
            throw std::overflow_error("Viterbi metric overflow");
          }
          const std::size_t candidate_metric = current_metric[state] + mismatch;
          const std::size_t destination = transition.next_state;
          const std::pair<std::size_t, std::uint8_t> candidate_tie{current_rank[state], bit};
          const std::pair<std::size_t, std::uint8_t> resident_tie{
              selected_predecessor[destination], selected_input[destination]};

          if (candidate_metric < next_metric[destination] ||
              (candidate_metric == next_metric[destination] &&
               candidate_tie < resident_tie)) {
            next_metric[destination] = candidate_metric;
            selected_predecessor[destination] = current_rank[state];
            selected_input[destination] = bit;
            predecessor[step * states + destination] = state;
            input_bit[step * states + destination] = bit;
          }
        }
      }

      std::fill(rank_slots.begin(), rank_slots.end(), infinity);
      for (std::size_t destination = 0; destination < states; ++destination) {
        if (next_metric[destination] == infinity) {
          continue;
        }
        const std::size_t key =
            2U * selected_predecessor[destination] +
            static_cast<std::size_t>(selected_input[destination]);
        if (key >= rank_slots.size() || rank_slots[key] != infinity) {
          throw std::logic_error("invalid Viterbi lexicographic-rank state");
        }
        rank_slots[key] = destination;
      }
      std::size_t rank = 0U;
      for (const std::size_t destination : rank_slots) {
        if (destination != infinity) {
          next_rank[destination] = rank;
          ++rank;
        }
      }

      current_metric.swap(next_metric);
      current_rank.swap(next_rank);
    }

    std::size_t best_state = 0U;
    std::size_t best_metric = current_metric[0U];
    std::size_t best_rank = current_rank[0U];
    for (std::size_t state = 1U; state < states; ++state) {
      if (current_metric[state] < best_metric ||
          (current_metric[state] == best_metric && current_rank[state] < best_rank)) {
        best_state = state;
        best_metric = current_metric[state];
        best_rank = current_rank[state];
      }
    }
    if (best_metric == infinity) {
      throw std::logic_error("no Viterbi path is reachable");
    }

    std::vector<std::uint8_t> message(steps, 0U);
    std::size_t state = best_state;
    for (std::size_t step = steps; step > 0U; --step) {
      const std::size_t index = (step - 1U) * states + state;
      message[step - 1U] = input_bit[index];
      state = predecessor[index];
    }

    return ViterbiDecodeResult{message, encode(message), best_metric, best_state};
  }

 private:
  struct Transition {
    std::size_t next_state;
    std::uint8_t first_output;
    std::uint8_t second_output;
  };

  static void validate_bit(std::uint8_t bit) {
    if (bit > 1U) {
      throw std::invalid_argument("binary message/received symbols must be 0 or 1");
    }
  }

  [[nodiscard]] static std::uint8_t parity(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(std::popcount(value) & 1);
  }

  [[nodiscard]] Transition transition_from(std::size_t state, std::uint8_t input) const noexcept {
    const std::uint32_t register_value =
        (static_cast<std::uint32_t>(state) << 1U) | static_cast<std::uint32_t>(input);
    const std::size_t state_mask = state_count() - 1U;
    return Transition{
        static_cast<std::size_t>(register_value) & state_mask,
        parity(register_value & first_generator_),
        parity(register_value & second_generator_)};
  }

  std::size_t constraint_length_;
  std::uint32_t first_generator_;
  std::uint32_t second_generator_;
};

}  // namespace algorithms::coding
