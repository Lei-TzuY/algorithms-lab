#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

struct BenesPermutationRoute {
  std::size_t width{};
  bool leaf_cross{};
  std::vector<std::uint8_t> input_cross;
  std::vector<std::uint8_t> output_cross;
  std::shared_ptr<const BenesPermutationRoute> upper;
  std::shared_ptr<const BenesPermutationRoute> lower;
};

[[nodiscard]] inline bool valid_benes_permutation_route(
    const BenesPermutationRoute& route) noexcept {
  const std::size_t n = route.width;
  if (n == 0U || n == 1U) {
    return !route.leaf_cross && route.input_cross.empty() &&
           route.output_cross.empty() && !route.upper && !route.lower;
  }

  if ((n & (n - 1U)) != 0U) {
    return false;
  }

  if (n == 2U) {
    return route.input_cross.empty() && route.output_cross.empty() &&
           !route.upper && !route.lower;
  }

  if (route.leaf_cross || route.input_cross.size() != n / 2U ||
      route.output_cross.size() != n / 2U || !route.upper ||
      !route.lower || route.upper->width != n / 2U ||
      route.lower->width != n / 2U) {
    return false;
  }

  for (const std::uint8_t setting : route.input_cross) {
    if (setting > 1U) {
      return false;
    }
  }
  for (const std::uint8_t setting : route.output_cross) {
    if (setting > 1U) {
      return false;
    }
  }

  return valid_benes_permutation_route(*route.upper) &&
         valid_benes_permutation_route(*route.lower);
}

namespace benes_detail {

inline void validate_permutation(
    const std::vector<std::size_t>& permutation) {
  const std::size_t n = permutation.size();
  if (n > 1U && (n & (n - 1U)) != 0U) {
    throw std::invalid_argument(
        "Beneš network width must be zero, one, or a power of two");
  }

  std::vector<bool> seen(n, false);
  for (const std::size_t output : permutation) {
    if (output >= n) {
      throw std::invalid_argument(
          "Beneš permutation output index out of range");
    }
    if (seen[output]) {
      throw std::invalid_argument(
          "Beneš permutation contains a duplicate output");
    }
    seen[output] = true;
  }
}

[[nodiscard]] inline BenesPermutationRoute build_route(
    const std::vector<std::size_t>& permutation) {
  const std::size_t n = permutation.size();
  BenesPermutationRoute route;
  route.width = n;

  if (n <= 1U) {
    return route;
  }

  if (n == 2U) {
    route.leaf_cross = permutation[0U] == 1U;
    return route;
  }

  std::vector<std::size_t> inverse(n, 0U);
  for (std::size_t input = 0U; input < n; ++input) {
    inverse[permutation[input]] = input;
  }

  // Each permutation wire is an edge between one input 2x2 switch and one
  // output 2x2 switch. Both switch families have degree two, so every connected
  // component is an even cycle (parallel-edge 2-cycles included). Alternate
  // edge colors select the upper/lower recursive subnetworks.
  std::vector<int> color(n, -1);
  std::vector<std::size_t> stack;
  stack.reserve(n);

  const auto require_color =
      [&](const std::size_t edge, const int expected,
          std::vector<std::size_t>& work) {
        if (color[edge] == -1) {
          color[edge] = expected;
          work.push_back(edge);
          return;
        }
        if (color[edge] != expected) {
          throw std::logic_error(
              "Beneš alternating-color invariant violated");
        }
      };

  for (std::size_t start = 0U; start < n; ++start) {
    if (color[start] != -1) {
      continue;
    }
    color[start] = 0;
    stack.push_back(start);

    while (!stack.empty()) {
      const std::size_t edge = stack.back();
      stack.pop_back();
      const int opposite = 1 - color[edge];

      require_color(edge ^ std::size_t{1}, opposite, stack);

      const std::size_t output = permutation[edge];
      const std::size_t output_mate_edge =
          inverse[output ^ std::size_t{1}];
      require_color(output_mate_edge, opposite, stack);
    }
  }

  route.input_cross.resize(n / 2U, 0U);
  route.output_cross.resize(n / 2U, 0U);
  std::vector<std::size_t> upper_permutation(n / 2U, 0U);
  std::vector<std::size_t> lower_permutation(n / 2U, 0U);

  for (std::size_t input_switch = 0U;
       input_switch < n / 2U; ++input_switch) {
    const std::size_t even_input = 2U * input_switch;
    const std::size_t odd_input = even_input + 1U;

    if (color[even_input] == color[odd_input]) {
      throw std::logic_error(
          "Beneš input switch received equal edge colors");
    }

    route.input_cross[input_switch] =
        static_cast<std::uint8_t>(color[even_input] == 1 ? 1U : 0U);

    const std::size_t upper_edge =
        color[even_input] == 0 ? even_input : odd_input;
    const std::size_t lower_edge =
        color[even_input] == 1 ? even_input : odd_input;

    upper_permutation[input_switch] =
        permutation[upper_edge] / 2U;
    lower_permutation[input_switch] =
        permutation[lower_edge] / 2U;
  }

  for (std::size_t output_switch = 0U;
       output_switch < n / 2U; ++output_switch) {
    const std::size_t even_output = 2U * output_switch;
    const std::size_t even_edge = inverse[even_output];
    const std::size_t odd_edge = inverse[even_output + 1U];

    if (color[even_edge] == color[odd_edge]) {
      throw std::logic_error(
          "Beneš output switch received equal edge colors");
    }

    route.output_cross[output_switch] =
        static_cast<std::uint8_t>(color[even_edge] == 1 ? 1U : 0U);
  }

  const auto validate_subpermutation =
      [](const std::vector<std::size_t>& sub) {
        std::vector<bool> seen(sub.size(), false);
        for (const std::size_t output : sub) {
          if (output >= sub.size() || seen[output]) {
            throw std::logic_error(
                "Beneš recursive permutation invariant violated");
          }
          seen[output] = true;
        }
      };

  validate_subpermutation(upper_permutation);
  validate_subpermutation(lower_permutation);

  route.upper = std::make_shared<const BenesPermutationRoute>(
      build_route(upper_permutation));
  route.lower = std::make_shared<const BenesPermutationRoute>(
      build_route(lower_permutation));
  return route;
}

template <class T>
[[nodiscard]] std::vector<T> apply_route_unchecked(
    const BenesPermutationRoute& route,
    const std::vector<T>& inputs) {
  const std::size_t n = route.width;
  if (n <= 1U) {
    return inputs;
  }

  if (n == 2U) {
    std::vector<T> outputs = inputs;
    if (route.leaf_cross) {
      std::swap(outputs[0U], outputs[1U]);
    }
    return outputs;
  }

  std::vector<T> upper_inputs;
  std::vector<T> lower_inputs;
  upper_inputs.reserve(n / 2U);
  lower_inputs.reserve(n / 2U);

  for (std::size_t input_switch = 0U;
       input_switch < n / 2U; ++input_switch) {
    const T& even = inputs[2U * input_switch];
    const T& odd = inputs[2U * input_switch + 1U];
    if (route.input_cross[input_switch] == 0U) {
      upper_inputs.push_back(even);
      lower_inputs.push_back(odd);
    } else {
      upper_inputs.push_back(odd);
      lower_inputs.push_back(even);
    }
  }

  std::vector<T> upper_outputs =
      apply_route_unchecked(*route.upper, upper_inputs);
  std::vector<T> lower_outputs =
      apply_route_unchecked(*route.lower, lower_inputs);

  std::vector<T> outputs = inputs;
  for (std::size_t output_switch = 0U;
       output_switch < n / 2U; ++output_switch) {
    if (route.output_cross[output_switch] == 0U) {
      outputs[2U * output_switch] = upper_outputs[output_switch];
      outputs[2U * output_switch + 1U] = lower_outputs[output_switch];
    } else {
      outputs[2U * output_switch] = lower_outputs[output_switch];
      outputs[2U * output_switch + 1U] = upper_outputs[output_switch];
    }
  }
  return outputs;
}

[[nodiscard]] inline std::size_t switch_count_unchecked(
    const BenesPermutationRoute& route) noexcept {
  if (route.width <= 1U) {
    return 0U;
  }
  if (route.width == 2U) {
    return 1U;
  }
  return route.input_cross.size() + route.output_cross.size() +
         switch_count_unchecked(*route.upper) +
         switch_count_unchecked(*route.lower);
}

inline void append_settings_preorder(
    const BenesPermutationRoute& route,
    std::vector<std::uint8_t>& settings) {
  if (route.width <= 1U) {
    return;
  }
  if (route.width == 2U) {
    settings.push_back(
        static_cast<std::uint8_t>(route.leaf_cross ? 1U : 0U));
    return;
  }

  settings.insert(settings.end(), route.input_cross.begin(),
                  route.input_cross.end());
  settings.insert(settings.end(), route.output_cross.begin(),
                  route.output_cross.end());
  append_settings_preorder(*route.upper, settings);
  append_settings_preorder(*route.lower, settings);
}

}  // namespace benes_detail

[[nodiscard]] inline BenesPermutationRoute benes_route_permutation(
    const std::vector<std::size_t>& permutation) {
  benes_detail::validate_permutation(permutation);
  BenesPermutationRoute route =
      benes_detail::build_route(permutation);
  if (!valid_benes_permutation_route(route)) {
    throw std::logic_error("constructed Beneš route is structurally invalid");
  }
  return route;
}

template <class T>
[[nodiscard]] std::vector<T> benes_apply(
    const BenesPermutationRoute& route,
    const std::vector<T>& inputs) {
  if (!valid_benes_permutation_route(route)) {
    throw std::invalid_argument("malformed Beneš permutation route");
  }
  if (inputs.size() != route.width) {
    throw std::invalid_argument(
        "Beneš payload width does not match route width");
  }
  return benes_detail::apply_route_unchecked(route, inputs);
}

[[nodiscard]] inline std::size_t benes_switch_count(
    const BenesPermutationRoute& route) {
  if (!valid_benes_permutation_route(route)) {
    throw std::invalid_argument("malformed Beneš permutation route");
  }
  return benes_detail::switch_count_unchecked(route);
}

[[nodiscard]] inline std::vector<std::uint8_t>
benes_switch_settings_preorder(
    const BenesPermutationRoute& route) {
  if (!valid_benes_permutation_route(route)) {
    throw std::invalid_argument("malformed Beneš permutation route");
  }
  std::vector<std::uint8_t> settings;
  settings.reserve(benes_switch_count(route));
  benes_detail::append_settings_preorder(route, settings);
  return settings;
}

}  // namespace algorithms::combinatorial
