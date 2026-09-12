#pragma once

#include "test_framework.hpp"
#include "algorithms/polynomials/extension_field.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::polynomials::PrimeFieldExtension;
using Poly = PrimeFieldExtension::Element;

void extension_trim(Poly& value) {
  while (!value.empty() && value.back() == 0U) {
    value.pop_back();
  }
}

std::uint64_t extension_small_add(std::uint64_t a, std::uint64_t b,
                                  std::uint64_t p) {
  return (a + b) % p;
}

std::uint64_t extension_small_subtract(std::uint64_t a, std::uint64_t b,
                                       std::uint64_t p) {
  return (a + p - b) % p;
}

Poly extension_oracle_add(Poly first, Poly second, std::uint64_t p) {
  first.resize(std::max(first.size(), second.size()), 0U);
  second.resize(first.size(), 0U);
  for (std::size_t index = 0U; index < first.size(); ++index) {
    first[index] = extension_small_add(first[index], second[index], p);
  }
  extension_trim(first);
  return first;
}

Poly extension_oracle_subtract(Poly first, Poly second, std::uint64_t p) {
  first.resize(std::max(first.size(), second.size()), 0U);
  second.resize(first.size(), 0U);
  for (std::size_t index = 0U; index < first.size(); ++index) {
    first[index] =
        extension_small_subtract(first[index], second[index], p);
  }
  extension_trim(first);
  return first;
}

Poly extension_oracle_multiply_reduce(const Poly& first, const Poly& second,
                                      const Poly& modulus, std::uint64_t p) {
  if (first.empty() || second.empty()) {
    return {};
  }
  Poly product(first.size() + second.size() - 1U, 0U);
  for (std::size_t left = 0U; left < first.size(); ++left) {
    for (std::size_t right = 0U; right < second.size(); ++right) {
      product[left + right] =
          (product[left + right] + (first[left] * second[right]) % p) % p;
    }
  }

  const std::size_t degree = modulus.size() - 1U;
  for (std::size_t size = product.size(); size > degree; --size) {
    const std::size_t high = size - 1U;
    const std::uint64_t factor = product[high];
    if (factor == 0U) {
      continue;
    }
    const std::size_t shift = high - degree;
    for (std::size_t index = 0U; index < degree; ++index) {
      product[shift + index] = extension_small_subtract(
          product[shift + index], (factor * modulus[index]) % p, p);
    }
    product[high] = 0U;
  }
  extension_trim(product);
  return product;
}

Poly extension_polynomial_remainder(Poly dividend, const Poly& divisor,
                                    std::uint64_t p) {
  extension_trim(dividend);
  const std::size_t divisor_degree = divisor.size() - 1U;
  while (dividend.size() >= divisor.size()) {
    const std::uint64_t factor = dividend.back();
    const std::size_t shift = dividend.size() - divisor.size();
    for (std::size_t index = 0U; index < divisor_degree; ++index) {
      dividend[shift + index] = extension_small_subtract(
          dividend[shift + index], (factor * divisor[index]) % p, p);
    }
    dividend.back() = 0U;
    extension_trim(dividend);
  }
  return dividend;
}

std::uint64_t extension_small_power(std::uint64_t base,
                                    std::size_t exponent) {
  std::uint64_t result = 1U;
  for (std::size_t index = 0U; index < exponent; ++index) {
    result *= base;
  }
  return result;
}

bool extension_naive_irreducible(const Poly& polynomial, std::uint64_t p) {
  if (polynomial.size() < 2U || polynomial.back() != 1U) {
    return false;
  }
  const std::size_t degree = polynomial.size() - 1U;
  for (std::size_t divisor_degree = 1U; divisor_degree <= degree / 2U;
       ++divisor_degree) {
    const std::uint64_t count = extension_small_power(p, divisor_degree);
    for (std::uint64_t code = 0U; code < count; ++code) {
      Poly divisor(divisor_degree + 1U, 0U);
      std::uint64_t value = code;
      for (std::size_t index = 0U; index < divisor_degree; ++index) {
        divisor[index] = value % p;
        value /= p;
      }
      divisor.back() = 1U;
      if (extension_polynomial_remainder(polynomial, divisor, p).empty()) {
        return false;
      }
    }
  }
  return true;
}

Poly extension_decode_element(std::uint64_t code, std::uint64_t p,
                              std::size_t degree) {
  Poly result(degree, 0U);
  for (std::size_t index = 0U; index < degree; ++index) {
    result[index] = code % p;
    code /= p;
  }
  extension_trim(result);
  return result;
}

Poly extension_oracle_inverse(const Poly& value, const Poly& modulus,
                              std::uint64_t p) {
  const std::size_t degree = modulus.size() - 1U;
  const std::uint64_t count = extension_small_power(p, degree);
  for (std::uint64_t code = 1U; code < count; ++code) {
    Poly candidate = extension_decode_element(code, p, degree);
    if (extension_oracle_multiply_reduce(value, candidate, modulus, p) ==
        Poly{1U}) {
      return candidate;
    }
  }
  throw std::runtime_error("extension-field oracle found no inverse");
}

TEST_CASE(extension_field_validates_irreducible_modulus_independently) {
  REQUIRE_THROWS_AS(PrimeFieldExtension(4U, Poly{1U, 1U, 1U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(PrimeFieldExtension(5U, Poly{1U}), std::invalid_argument);
  REQUIRE_THROWS_AS(PrimeFieldExtension(5U, Poly{1U, 0U, 2U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(PrimeFieldExtension(5U, Poly{4U, 0U, 1U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(PrimeFieldExtension(5U, Poly{1U, 2U, 1U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(PrimeFieldExtension(5U, Poly{5U, 0U, 1U}),
                    std::invalid_argument);

  std::mt19937_64 rng(0xE17E51ULL);
  for (const std::uint64_t p : {2U, 3U}) {
    for (std::size_t trial = 0U; trial < 240U; ++trial) {
      const std::size_t degree =
          1U + static_cast<std::size_t>(rng() % 4U);
      Poly modulus(degree + 1U, 0U);
      for (std::size_t index = 0U; index < degree; ++index) {
        modulus[index] = rng() % p;
      }
      modulus.back() = 1U;
      const bool expected = extension_naive_irreducible(modulus, p);
      bool accepted = true;
      try {
        PrimeFieldExtension field(p, modulus);
        REQUIRE_EQ(field.modulus(), modulus);
      } catch (const std::invalid_argument&) {
        accepted = false;
      }
      REQUIRE_EQ(accepted, expected);
    }
  }
}

TEST_CASE(extension_field_known_arithmetic_and_canonical_representatives) {
  PrimeFieldExtension binary(2U, Poly{1U, 1U, 0U, 1U});
  const Poly alpha{0U, 1U};
  REQUIRE_EQ(binary.power(alpha, 3U), (Poly{1U, 1U}));
  REQUIRE_EQ(binary.inverse(alpha), (Poly{1U, 0U, 1U}));
  REQUIRE_EQ(binary.multiply(alpha, binary.inverse(alpha)), (Poly{1U}));
  REQUIRE_EQ(binary.power(alpha, 7U), (Poly{1U}));

  PrimeFieldExtension five(5U, Poly{2U, 0U, 1U});
  const Poly one_plus_x{1U, 1U};
  REQUIRE_EQ(five.multiply(one_plus_x, one_plus_x), (Poly{4U, 2U}));
  REQUIRE_EQ(five.inverse(one_plus_x), (Poly{2U, 3U}));
  REQUIRE_EQ(five.divide((Poly{4U, 2U}), one_plus_x), one_plus_x);
  REQUIRE_EQ(five.add((Poly{4U, 4U}), (Poly{1U, 1U})), (Poly{}));
  REQUIRE_EQ(five.subtract((Poly{}), (Poly{1U, 1U})), (Poly{4U, 4U}));
  REQUIRE_EQ(five.add((Poly{1U, 2U, 0U}), (Poly{})), (Poly{1U, 2U}));
}

TEST_CASE(extension_field_rejects_noncanonical_elements_and_zero_inverse) {
  PrimeFieldExtension field(5U, Poly{2U, 0U, 1U});
  REQUIRE_THROWS_AS(field.add((Poly{5U}), (Poly{})), std::invalid_argument);
  REQUIRE_THROWS_AS(field.multiply((Poly{0U, 0U, 1U}), (Poly{1U})),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(field.inverse((Poly{})), std::domain_error);
  REQUIRE_THROWS_AS(field.divide((Poly{1U}), (Poly{})), std::domain_error);

  constexpr std::uint64_t prime = 18446744073709551557ULL;
  PrimeFieldExtension full_width(prime, Poly{1U, 1U});
  const Poly minus_two{prime - 2U};
  const Poly inverse{(prime - 1U) / 2U};
  REQUIRE_EQ(full_width.inverse(minus_two), inverse);
  REQUIRE_EQ(full_width.multiply(minus_two, inverse), (Poly{1U}));
}

TEST_CASE(extension_field_exhaustive_small_field_differential) {
  struct Spec {
    std::uint64_t prime;
    Poly modulus;
  };
  const std::vector<Spec> specs{{2U, {1U, 1U, 0U, 1U}},
                                {3U, {1U, 0U, 1U}},
                                {5U, {2U, 0U, 1U}}};
  for (const Spec& spec : specs) {
    PrimeFieldExtension field(spec.prime, spec.modulus);
    const std::size_t degree = spec.modulus.size() - 1U;
    const std::uint64_t count = extension_small_power(spec.prime, degree);
    for (std::uint64_t first_code = 0U; first_code < count; ++first_code) {
      const Poly first =
          extension_decode_element(first_code, spec.prime, degree);
      for (std::uint64_t second_code = 0U; second_code < count;
           ++second_code) {
        const Poly second =
            extension_decode_element(second_code, spec.prime, degree);
        REQUIRE_EQ(field.add(first, second),
                   extension_oracle_add(first, second, spec.prime));
        REQUIRE_EQ(field.subtract(first, second),
                   extension_oracle_subtract(first, second, spec.prime));
        REQUIRE_EQ(field.multiply(first, second),
                   extension_oracle_multiply_reduce(
                       first, second, spec.modulus, spec.prime));
      }
      if (!first.empty()) {
        const Poly expected_inverse =
            extension_oracle_inverse(first, spec.modulus, spec.prime);
        REQUIRE_EQ(field.inverse(first), expected_inverse);
        REQUIRE_EQ(field.multiply(first, expected_inverse), (Poly{1U}));
      }
    }
  }
}

TEST_CASE(extension_field_randomized_power_and_field_laws) {
  PrimeFieldExtension field(5U, Poly{2U, 0U, 1U});
  const Poly modulus{2U, 0U, 1U};
  std::mt19937_64 rng(0xF13DCAFEULL);
  for (std::size_t trial = 0U; trial < 1000U; ++trial) {
    const Poly first = extension_decode_element(rng() % 25U, 5U, 2U);
    const Poly second = extension_decode_element(rng() % 25U, 5U, 2U);
    const Poly third = extension_decode_element(rng() % 25U, 5U, 2U);
    REQUIRE_EQ(field.multiply(first, field.add(second, third)),
               field.add(field.multiply(first, second),
                         field.multiply(first, third)));
    REQUIRE_EQ(field.multiply(first, second), field.multiply(second, first));

    const std::uint64_t exponent = rng() % 40U;
    Poly repeated{1U};
    for (std::uint64_t step = 0U; step < exponent; ++step) {
      repeated =
          extension_oracle_multiply_reduce(repeated, first, modulus, 5U);
    }
    REQUIRE_EQ(field.power(first, exponent), repeated);
  }
}

}  // namespace
