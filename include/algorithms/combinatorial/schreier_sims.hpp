#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

using Permutation = std::vector<std::size_t>;

struct SchreierSimsLevel {
  std::size_t base_point{};
  std::vector<std::size_t> orbit;
  std::size_t stabilizer_generator_count{};
};

struct PermutationMembershipResult {
  bool member{};
  std::size_t first_failed_level{};
  Permutation residue;
};

namespace schreier_sims_detail {

[[nodiscard]] inline Permutation identity_permutation(std::size_t degree) {
  Permutation result(degree);
  for (std::size_t index = 0; index < degree; ++index) {
    result[index] = index;
  }
  return result;
}

inline void validate_permutation(std::span<const std::size_t> permutation,
                                 std::size_t degree) {
  if (permutation.size() != degree) {
    throw std::invalid_argument("permutation degree mismatch");
  }
  std::vector<bool> seen(degree, false);
  for (const std::size_t value : permutation) {
    if (value >= degree || seen[value]) {
      throw std::invalid_argument(
          "permutation must be a bijection of [0, degree)");
    }
    seen[value] = true;
  }
}

[[nodiscard]] inline bool is_identity(std::span<const std::size_t> permutation) {
  for (std::size_t index = 0; index < permutation.size(); ++index) {
    if (permutation[index] != index) {
      return false;
    }
  }
  return true;
}

// Composition uses the function convention compose(left, right) = left ∘ right.
[[nodiscard]] inline Permutation compose(std::span<const std::size_t> left,
                                         std::span<const std::size_t> right) {
  Permutation result(left.size());
  for (std::size_t index = 0; index < left.size(); ++index) {
    result[index] = left[right[index]];
  }
  return result;
}

[[nodiscard]] inline Permutation inverse(std::span<const std::size_t> permutation) {
  Permutation result(permutation.size());
  for (std::size_t index = 0; index < permutation.size(); ++index) {
    result[permutation[index]] = index;
  }
  return result;
}

[[nodiscard]] inline std::vector<Permutation> normalize_generators(
    std::size_t degree, std::vector<Permutation> generators) {
  std::vector<Permutation> normalized;
  if (generators.size() > normalized.max_size() / 2U) {
    throw std::length_error("too many permutation generators");
  }
  normalized.reserve(generators.size() * 2U);
  for (auto& generator : generators) {
    validate_permutation(generator, degree);
    if (!is_identity(generator)) {
      normalized.push_back(generator);
      normalized.push_back(inverse(generator));
    }
  }
  std::sort(normalized.begin(), normalized.end());
  normalized.erase(std::unique(normalized.begin(), normalized.end()),
                   normalized.end());
  return normalized;
}

struct InternalLevel {
  SchreierSimsLevel public_level;
  std::vector<std::optional<Permutation>> transversal;
};

}  // namespace schreier_sims_detail

class SchreierSimsGroup {
 public:
  SchreierSimsGroup(std::size_t degree, std::vector<Permutation> generators)
      : degree_(degree), generators_(schreier_sims_detail::normalize_generators(
                             degree, std::move(generators))) {
    build_chain();
  }

  [[nodiscard]] std::size_t degree() const noexcept { return degree_; }
  [[nodiscard]] std::size_t normalized_generator_count() const noexcept {
    return generators_.size();
  }

  [[nodiscard]] const std::vector<SchreierSimsLevel>& levels() const noexcept {
    return public_levels_;
  }

  // Orbit-stabilizer gives |G| = product(level.orbit.size()).  Returning the
  // factors keeps that statement exact even when the integer product would not
  // fit uint64_t.
  [[nodiscard]] std::vector<std::size_t> order_factors() const {
    std::vector<std::size_t> factors;
    factors.reserve(public_levels_.size());
    for (const auto& level : public_levels_) {
      factors.push_back(level.orbit.size());
    }
    return factors;
  }

  [[nodiscard]] std::uint64_t order_u64() const {
    std::uint64_t order = 1;
    for (const auto& level : public_levels_) {
      const auto factor = static_cast<std::uint64_t>(level.orbit.size());
      if (factor != 0U &&
          order > std::numeric_limits<std::uint64_t>::max() / factor) {
        throw std::overflow_error("permutation-group order exceeds uint64_t");
      }
      order *= factor;
    }
    return order;
  }

  [[nodiscard]] PermutationMembershipResult sift(
      std::span<const std::size_t> candidate) const {
    schreier_sims_detail::validate_permutation(candidate, degree_);
    Permutation residue(candidate.begin(), candidate.end());

    for (std::size_t level_index = 0; level_index < internal_levels_.size();
         ++level_index) {
      const auto& level = internal_levels_[level_index];
      const std::size_t image = residue[level.public_level.base_point];
      const auto& transversal = level.transversal[image];
      if (!transversal.has_value()) {
        return PermutationMembershipResult{false, level_index,
                                           std::move(residue)};
      }
      residue = schreier_sims_detail::compose(
          schreier_sims_detail::inverse(*transversal), residue);
    }

    const bool member = schreier_sims_detail::is_identity(residue);
    return PermutationMembershipResult{member, public_levels_.size(),
                                       std::move(residue)};
  }

  [[nodiscard]] bool contains(std::span<const std::size_t> candidate) const {
    return sift(candidate).member;
  }

 private:
  void build_chain() {
    internal_levels_.clear();
    public_levels_.clear();
    internal_levels_.reserve(degree_);
    public_levels_.reserve(degree_);

    std::vector<Permutation> current_generators = generators_;
    const Permutation identity =
        schreier_sims_detail::identity_permutation(degree_);

    for (std::size_t base = 0; base < degree_; ++base) {
      schreier_sims_detail::InternalLevel level;
      level.public_level.base_point = base;
      level.transversal.resize(degree_);
      level.transversal[base] = identity;

      std::queue<std::size_t> pending;
      pending.push(base);
      std::vector<std::size_t> discovery_order;
      discovery_order.push_back(base);

      while (!pending.empty()) {
        const std::size_t alpha = pending.front();
        pending.pop();
        const Permutation& t_alpha = *level.transversal[alpha];
        for (const auto& generator : current_generators) {
          const std::size_t beta = generator[alpha];
          if (!level.transversal[beta].has_value()) {
            level.transversal[beta] =
                schreier_sims_detail::compose(generator, t_alpha);
            pending.push(beta);
            discovery_order.push_back(beta);
          }
        }
      }

      level.public_level.orbit = discovery_order;
      std::sort(level.public_level.orbit.begin(), level.public_level.orbit.end());

      std::vector<Permutation> stabilizer_generators;
      if (!current_generators.empty()) {
        if (discovery_order.size() >
            stabilizer_generators.max_size() / current_generators.size()) {
          throw std::length_error("Schreier generator set is too large");
        }
        stabilizer_generators.reserve(discovery_order.size() *
                                      current_generators.size());
      }
      for (const std::size_t alpha : discovery_order) {
        const Permutation& t_alpha = *level.transversal[alpha];
        for (const auto& generator : current_generators) {
          const std::size_t beta = generator[alpha];
          const Permutation& t_beta = *level.transversal[beta];
          Permutation schreier = schreier_sims_detail::compose(
              schreier_sims_detail::inverse(t_beta),
              schreier_sims_detail::compose(generator, t_alpha));
          if (!schreier_sims_detail::is_identity(schreier)) {
            stabilizer_generators.push_back(std::move(schreier));
          }
        }
      }
      current_generators = schreier_sims_detail::normalize_generators(
          degree_, std::move(stabilizer_generators));
      level.public_level.stabilizer_generator_count = current_generators.size();

      public_levels_.push_back(level.public_level);
      internal_levels_.push_back(std::move(level));
    }
  }

  std::size_t degree_{};
  std::vector<Permutation> generators_;
  std::vector<SchreierSimsLevel> public_levels_;
  std::vector<schreier_sims_detail::InternalLevel> internal_levels_;
};

}  // namespace algorithms::combinatorial
