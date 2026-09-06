#include <chrono>
#include <cstddef>
#include <iostream>
#include <random>
#include <vector>

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/sorting/merge_sort.hpp"
#include "algorithms/sorting/quick_sort.hpp"

namespace {
using Clock = std::chrono::steady_clock;

template <typename Function>
long long elapsed_microseconds(Function function) {
  const auto start = Clock::now();
  function();
  const auto end = Clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start)
      .count();
}
}  // namespace

int main() {
  constexpr std::size_t element_count = 100'000;
  std::mt19937 rng(0xB3A5u);
  std::uniform_int_distribution<int> distribution(-1'000'000, 1'000'000);
  std::vector<int> input;
  input.reserve(element_count);
  for (std::size_t i = 0; i < element_count; ++i) {
    input.push_back(distribution(rng));
  }

  auto merge_values = input;
  const auto merge_us = elapsed_microseconds(
      [&] { algorithms::sorting::merge_sort(merge_values); });

  auto quick_values = input;
  const auto quick_us = elapsed_microseconds(
      [&] { algorithms::sorting::quick_sort(quick_values); });

  const auto heap_us = elapsed_microseconds([&] {
    algorithms::data_structures::BinaryHeap<int> heap;
    for (int value : input) {
      heap.push(value);
    }
    while (!heap.empty()) {
      static_cast<void>(heap.pop());
    }
  });

  std::cout << "seed=0xB3A5 n=" << element_count << '\n'
            << "merge_sort_us=" << merge_us << '\n'
            << "quick_sort_us=" << quick_us << '\n'
            << "heap_push_pop_us=" << heap_us << '\n';
}
