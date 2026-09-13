#pragma once

#include "algorithms/polynomials/formal_power_series.hpp"
#include "algorithms/polynomials/number_theoretic_transform.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::polynomials {
namespace multipoint_detail {

inline void normalize_and_trim(std::vector<std::uint64_t>& polynomial) {
  for (auto& coefficient : polynomial) {
    coefficient %= kNttModulus;
  }
  while (!polynomial.empty() && polynomial.back() == 0U) {
    polynomial.pop_back();
  }
}

[[nodiscard]] inline std::vector<std::uint64_t> multiply(
    const std::vector<std::uint64_t>& left,
    const std::vector<std::uint64_t>& right) {
  auto product = convolution_mod_998244353(left, right);
  normalize_and_trim(product);
  return product;
}

[[nodiscard]] inline std::vector<std::uint64_t> polynomial_mod(
    std::vector<std::uint64_t> dividend,
    std::vector<std::uint64_t> divisor) {
  normalize_and_trim(dividend);
  normalize_and_trim(divisor);
  if (divisor.empty()) {
    throw std::invalid_argument("polynomial divisor must be non-zero");
  }
  if (dividend.size() < divisor.size()) {
    return dividend;
  }
  if (divisor.back() != 1U) {
    throw std::invalid_argument("multipoint product-tree divisor must be monic");
  }

  const std::size_t quotient_size = dividend.size() - divisor.size() + 1U;
  std::vector<std::uint64_t> reversed_dividend(quotient_size, 0U);
  for (std::size_t index = 0U; index < quotient_size; ++index) {
    reversed_dividend[index] = dividend[dividend.size() - 1U - index];
  }

  std::vector<std::uint64_t> reversed_divisor(divisor.rbegin(), divisor.rend());
  auto inverse = formal_power_series_inverse(reversed_divisor, quotient_size);
  auto reversed_quotient =
      convolution_mod_998244353(reversed_dividend, inverse);
  reversed_quotient.resize(quotient_size);

  std::vector<std::uint64_t> quotient(reversed_quotient.rbegin(),
                                      reversed_quotient.rend());
  auto removed = convolution_mod_998244353(quotient, divisor);

  const std::size_t remainder_size = divisor.size() - 1U;
  std::vector<std::uint64_t> remainder(remainder_size, 0U);
  for (std::size_t index = 0U; index < remainder_size; ++index) {
    const std::uint64_t left = index < dividend.size() ? dividend[index] : 0U;
    const std::uint64_t right = index < removed.size() ? removed[index] : 0U;
    remainder[index] = left >= right ? left - right : left + kNttModulus - right;
  }
  normalize_and_trim(remainder);
  return remainder;
}

class ProductTree {
 public:
  explicit ProductTree(std::span<const std::uint64_t> points)
      : points_(points.begin(), points.end()) {
    if (points.size() >= kMaximumNttSize) {
      throw std::length_error("multipoint product tree exceeds NTT size limit");
    }
    tree_.resize(points.empty() ? 0U : points.size() * 4U);
    for (auto& point : points_) {
      point %= kNttModulus;
    }
    if (!points_.empty()) {
      build(1U, 0U, points_.size());
    }
  }

  [[nodiscard]] const std::vector<std::uint64_t>& root_polynomial() const {
    if (tree_.empty()) {
      throw std::logic_error("empty product tree has no root polynomial");
    }
    return tree_[1U];
  }

  void evaluate(const std::vector<std::uint64_t>& polynomial,
                std::vector<std::uint64_t>& result) const {
    if (points_.empty()) {
      return;
    }
    auto root_remainder = polynomial_mod(polynomial, tree_[1U]);
    evaluate_node(1U, 0U, points_.size(), root_remainder, result);
  }

 private:
  std::vector<std::uint64_t> points_;
  std::vector<std::vector<std::uint64_t>> tree_;

  void build(std::size_t node, std::size_t begin, std::size_t end) {
    if (end - begin == 1U) {
      const std::uint64_t point = points_[begin];
      tree_[node] = {point == 0U ? 0U : kNttModulus - point, 1U};
      return;
    }
    const std::size_t middle = begin + (end - begin) / 2U;
    build(node * 2U, begin, middle);
    build(node * 2U + 1U, middle, end);
    tree_[node] = multiply(tree_[node * 2U], tree_[node * 2U + 1U]);
  }

  void evaluate_node(std::size_t node, std::size_t begin, std::size_t end,
                     const std::vector<std::uint64_t>& remainder,
                     std::vector<std::uint64_t>& result) const {
    if (end - begin == 1U) {
      result[begin] = remainder.empty() ? 0U : remainder[0] % kNttModulus;
      return;
    }
    const std::size_t middle = begin + (end - begin) / 2U;
    const auto left_remainder = polynomial_mod(remainder, tree_[node * 2U]);
    const auto right_remainder = polynomial_mod(remainder, tree_[node * 2U + 1U]);
    evaluate_node(node * 2U, begin, middle, left_remainder, result);
    evaluate_node(node * 2U + 1U, middle, end, right_remainder, result);
  }
};

}  // namespace multipoint_detail

// Evaluates a little-endian coefficient vector at every supplied point in
// Z/998244353Z. Points and coefficients are reduced modulo the field modulus.
// Duplicate points are preserved and therefore produce duplicate outputs.
//
// Construction uses a subproduct tree of monic factors (x - point). The input
// polynomial is reduced at the root, then remainders are propagated down the
// tree. Polynomial remainder uses reversal plus the sealed FPS inverse / NTT
// convolution substrate rather than coefficient-by-coefficient long division.
// Existing NTT/FPS length limits are part of this API's representability bound.
[[nodiscard]] inline std::vector<std::uint64_t>
multipoint_evaluate_mod_998244353(
    std::span<const std::uint64_t> coefficients,
    std::span<const std::uint64_t> points) {
  if (points.empty()) {
    return {};
  }

  std::vector<std::uint64_t> polynomial(coefficients.begin(), coefficients.end());
  multipoint_detail::normalize_and_trim(polynomial);
  std::vector<std::uint64_t> result(points.size(), 0U);
  if (polynomial.empty()) {
    return result;
  }

  multipoint_detail::ProductTree tree(points);
  tree.evaluate(polynomial, result);
  return result;
}

}  // namespace algorithms::polynomials
