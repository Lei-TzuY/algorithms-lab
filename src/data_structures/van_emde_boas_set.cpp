#include "algorithms/data_structures/van_emde_boas_set.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct VanEmdeBoasSet::Node {
  explicit Node(unsigned bit_count) : bits(bit_count) {
    if (bits > 1U) {
      upper_bits = (bits + 1U) / 2U;
      lower_bits = bits / 2U;
      summary = std::make_unique<Node>(upper_bits);
      const std::size_t cluster_count = std::size_t{1} << upper_bits;
      clusters.reserve(cluster_count);
      for (std::size_t index = 0; index < cluster_count; ++index) {
        clusters.push_back(std::make_unique<Node>(lower_bits));
      }
    }
  }

  [[nodiscard]] std::uint32_t high(std::uint32_t value) const noexcept {
    return value >> lower_bits;
  }

  [[nodiscard]] std::uint32_t low(std::uint32_t value) const noexcept {
    const std::uint32_t mask = (std::uint32_t{1} << lower_bits) - 1U;
    return value & mask;
  }

  [[nodiscard]] std::uint32_t combine(std::uint32_t high_part,
                                      std::uint32_t low_part) const noexcept {
    return (high_part << lower_bits) | low_part;
  }

  void empty_insert(std::uint32_t value) noexcept {
    minimum = value;
    maximum = value;
  }

  [[nodiscard]] bool contains(std::uint32_t value) const {
    if (minimum == value || maximum == value) {
      return true;
    }
    if (bits == 1U || !minimum.has_value()) {
      return false;
    }
    const std::uint32_t cluster_index = high(value);
    return clusters[static_cast<std::size_t>(cluster_index)]->contains(low(value));
  }

  void insert_new(std::uint32_t value) {
    if (!minimum.has_value()) {
      empty_insert(value);
      return;
    }

    if (value < *minimum) {
      std::swap(value, *minimum);
    }

    if (bits > 1U) {
      const std::uint32_t cluster_index = high(value);
      Node& cluster = *clusters[static_cast<std::size_t>(cluster_index)];
      const std::uint32_t low_value = low(value);
      if (!cluster.minimum.has_value()) {
        summary->insert_new(cluster_index);
        cluster.empty_insert(low_value);
      } else {
        cluster.insert_new(low_value);
      }
    }

    if (value > *maximum) {
      maximum = value;
    }
  }

  void erase_existing(std::uint32_t value) {
    if (minimum == maximum) {
      minimum.reset();
      maximum.reset();
      return;
    }

    if (bits == 1U) {
      const std::uint32_t remaining = (value == 0U) ? 1U : 0U;
      minimum = remaining;
      maximum = remaining;
      return;
    }

    if (value == *minimum) {
      const std::uint32_t first_cluster = *summary->minimum;
      const Node& cluster = *clusters[static_cast<std::size_t>(first_cluster)];
      value = combine(first_cluster, *cluster.minimum);
      minimum = value;
    }

    const std::uint32_t cluster_index = high(value);
    Node& cluster = *clusters[static_cast<std::size_t>(cluster_index)];
    cluster.erase_existing(low(value));

    if (!cluster.minimum.has_value()) {
      summary->erase_existing(cluster_index);
      if (value == *maximum) {
        if (!summary->maximum.has_value()) {
          maximum = minimum;
        } else {
          const std::uint32_t last_cluster = *summary->maximum;
          const Node& last = *clusters[static_cast<std::size_t>(last_cluster)];
          maximum = combine(last_cluster, *last.maximum);
        }
      }
    } else if (value == *maximum) {
      maximum = combine(cluster_index, *cluster.maximum);
    }
  }

  [[nodiscard]] std::optional<std::uint32_t> successor(std::uint32_t value) const {
    if (!minimum.has_value()) {
      return std::nullopt;
    }
    if (value < *minimum) {
      return minimum;
    }
    if (bits == 1U) {
      if (value == 0U && maximum == 1U) {
        return 1U;
      }
      return std::nullopt;
    }

    const std::uint32_t cluster_index = high(value);
    const Node& cluster = *clusters[static_cast<std::size_t>(cluster_index)];
    const std::uint32_t low_value = low(value);
    if (cluster.maximum.has_value() && low_value < *cluster.maximum) {
      const auto low_successor = cluster.successor(low_value);
      return combine(cluster_index, *low_successor);
    }

    const auto successor_cluster = summary->successor(cluster_index);
    if (!successor_cluster.has_value()) {
      return std::nullopt;
    }
    const Node& next = *clusters[static_cast<std::size_t>(*successor_cluster)];
    return combine(*successor_cluster, *next.minimum);
  }

  [[nodiscard]] std::optional<std::uint32_t> predecessor(std::uint32_t value) const {
    if (!maximum.has_value()) {
      return std::nullopt;
    }
    if (value > *maximum) {
      return maximum;
    }
    if (bits == 1U) {
      if (value == 1U && minimum == 0U) {
        return 0U;
      }
      return std::nullopt;
    }

    const std::uint32_t cluster_index = high(value);
    const Node& cluster = *clusters[static_cast<std::size_t>(cluster_index)];
    const std::uint32_t low_value = low(value);
    if (cluster.minimum.has_value() && low_value > *cluster.minimum) {
      const auto low_predecessor = cluster.predecessor(low_value);
      return combine(cluster_index, *low_predecessor);
    }

    const auto predecessor_cluster = summary->predecessor(cluster_index);
    if (predecessor_cluster.has_value()) {
      const Node& previous = *clusters[static_cast<std::size_t>(*predecessor_cluster)];
      return combine(*predecessor_cluster, *previous.maximum);
    }
    if (minimum.has_value() && value > *minimum) {
      return minimum;
    }
    return std::nullopt;
  }

  unsigned bits{};
  unsigned upper_bits{};
  unsigned lower_bits{};
  std::optional<std::uint32_t> minimum;
  std::optional<std::uint32_t> maximum;
  std::unique_ptr<Node> summary;
  std::vector<std::unique_ptr<Node>> clusters;
};

VanEmdeBoasSet::VanEmdeBoasSet(unsigned universe_bits) {
  if (universe_bits < 1U || universe_bits > 16U) {
    throw std::invalid_argument("van Emde Boas universe bits must be in [1,16]");
  }
  universe_size_ = std::uint32_t{1} << universe_bits;
  root_ = std::make_unique<Node>(universe_bits);
}

VanEmdeBoasSet::~VanEmdeBoasSet() = default;

std::uint32_t VanEmdeBoasSet::universe_size() const noexcept { return universe_size_; }
std::size_t VanEmdeBoasSet::size() const noexcept { return size_; }
bool VanEmdeBoasSet::empty() const noexcept { return size_ == 0U; }

void VanEmdeBoasSet::validate_key(std::uint32_t key) const {
  if (key >= universe_size_) {
    throw std::out_of_range("van Emde Boas key outside configured universe");
  }
}

bool VanEmdeBoasSet::insert(std::uint32_t key) {
  validate_key(key);
  if (root_->contains(key)) {
    return false;
  }
  root_->insert_new(key);
  ++size_;
  return true;
}

bool VanEmdeBoasSet::erase(std::uint32_t key) {
  validate_key(key);
  if (!root_->contains(key)) {
    return false;
  }
  root_->erase_existing(key);
  --size_;
  return true;
}

bool VanEmdeBoasSet::contains(std::uint32_t key) const {
  validate_key(key);
  return root_->contains(key);
}

std::optional<std::uint32_t> VanEmdeBoasSet::minimum() const noexcept {
  return root_->minimum;
}
std::optional<std::uint32_t> VanEmdeBoasSet::maximum() const noexcept {
  return root_->maximum;
}

std::optional<std::uint32_t> VanEmdeBoasSet::predecessor(std::uint32_t key) const {
  validate_key(key);
  return root_->predecessor(key);
}

std::optional<std::uint32_t> VanEmdeBoasSet::successor(std::uint32_t key) const {
  validate_key(key);
  return root_->successor(key);
}

}  // namespace algorithms::data_structures
