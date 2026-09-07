#include "algorithms/streaming/misra_gries.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace algorithms::streaming {

MisraGries::MisraGries(std::size_t k) : k_(k) {
  if (k < 2U) {
    throw std::invalid_argument("Misra-Gries requires k >= 2");
  }
}

void MisraGries::update(std::int64_t item) {
  if (processed_count_ == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error("Misra-Gries processed-count overflow");
  }

  for (auto& counter : counters_) {
    if (counter.item == item) {
      if (counter.residual_count == std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("Misra-Gries counter overflow");
      }
      ++counter.residual_count;
      ++processed_count_;
      return;
    }
  }

  if (counters_.size() < k_ - 1U) {
    counters_.push_back(MisraGriesCounter{item, 1U});
    ++processed_count_;
    return;
  }

  if (decrement_rounds_ == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error("Misra-Gries decrement-round overflow");
  }

  for (auto& counter : counters_) {
    --counter.residual_count;
  }
  counters_.erase(
      std::remove_if(counters_.begin(), counters_.end(),
                     [](const MisraGriesCounter& counter) {
                       return counter.residual_count == 0U;
                     }),
      counters_.end());
  ++decrement_rounds_;
  ++processed_count_;
}

std::size_t MisraGries::k() const noexcept { return k_; }

std::size_t MisraGries::processed_count() const noexcept {
  return processed_count_;
}

std::size_t MisraGries::decrement_rounds() const noexcept {
  return decrement_rounds_;
}

std::size_t MisraGries::counter_count() const noexcept {
  return counters_.size();
}

std::size_t MisraGries::residual_count(std::int64_t item) const noexcept {
  for (const auto& counter : counters_) {
    if (counter.item == item) {
      return counter.residual_count;
    }
  }
  return 0U;
}

bool MisraGries::is_candidate(std::int64_t item) const noexcept {
  return residual_count(item) != 0U;
}

MisraGriesSummary MisraGries::summary() const {
  auto ordered = counters_;
  std::sort(ordered.begin(), ordered.end(),
            [](const MisraGriesCounter& lhs, const MisraGriesCounter& rhs) {
              return lhs.item < rhs.item;
            });
  return MisraGriesSummary{k_, processed_count_, decrement_rounds_,
                           std::move(ordered)};
}

MisraGriesSummary misra_gries_summary(std::span<const std::int64_t> stream,
                                      std::size_t k) {
  MisraGries state(k);
  for (const auto item : stream) {
    state.update(item);
  }
  return state.summary();
}

}  // namespace algorithms::streaming
