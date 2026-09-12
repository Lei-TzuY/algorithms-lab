#include "algorithms/automata/cyk_parser.hpp"
#include "test_framework.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace algorithms::automata;

bool has_terminal_rule(const CnfGrammar& g, CfgNonterminal lhs,
                       std::uint8_t terminal) {
  for (const auto& r : g.terminal_rules) {
    if (r.lhs == lhs && r.terminal == terminal) return true;
  }
  return false;
}

bool has_binary_rule(const CnfGrammar& g, CfgNonterminal lhs,
                     CfgNonterminal left, CfgNonterminal right) {
  for (const auto& r : g.binary_rules) {
    if (r.lhs == lhs && r.left == left && r.right == right) return true;
  }
  return false;
}

bool replay(const CnfGrammar& g, std::string_view input,
            const CykParseResult& result) {
  if (!result.accepted) return result.nodes.empty() && !result.root.has_value();
  if (input.empty()) {
    return g.start_accepts_empty && result.nodes.empty() && !result.root.has_value();
  }
  if (!result.root || *result.root >= result.nodes.size()) return false;
  const auto& root = result.nodes[*result.root];
  if (root.nonterminal != g.start || root.begin != 0 || root.end != input.size()) return false;
  for (std::size_t i = 0; i < result.nodes.size(); ++i) {
    const auto& node = result.nodes[i];
    if (node.nonterminal >= g.nonterminal_count || node.begin >= node.end ||
        node.end > input.size()) return false;
    if (node.kind == CykNodeKind::Terminal) {
      if (node.end != node.begin + 1U ||
          node.terminal != static_cast<std::uint8_t>(static_cast<unsigned char>(input[node.begin])) ||
          !has_terminal_rule(g, node.nonterminal, node.terminal)) return false;
    } else {
      if (node.left_child >= i || node.right_child >= i) return false;
      const auto& left = result.nodes[node.left_child];
      const auto& right = result.nodes[node.right_child];
      if (node.split <= node.begin || node.split >= node.end ||
          left.begin != node.begin || left.end != node.split ||
          right.begin != node.split || right.end != node.end ||
          !has_binary_rule(g, node.nonterminal, left.nonterminal, right.nonterminal)) return false;
    }
  }
  return true;
}

struct Oracle {
  const CnfGrammar& g;
  std::string_view input;
  std::vector<std::int8_t> memo;
  std::size_t n;

  std::size_t idx(std::size_t begin, std::size_t len, std::size_t nt) const {
    return ((begin * n) + (len - 1U)) * g.nonterminal_count + nt;
  }

  bool derives(std::size_t nt, std::size_t begin, std::size_t len) {
    auto& m = memo[idx(begin, len, nt)];
    if (m != -1) return m != 0;
    bool ok = false;
    if (len == 1U) {
      const auto byte = static_cast<std::uint8_t>(static_cast<unsigned char>(input[begin]));
      ok = has_terminal_rule(g, nt, byte);
    } else {
      for (const auto& r : g.binary_rules) {
        if (r.lhs != nt) continue;
        for (std::size_t split = 1; split < len; ++split) {
          if (derives(r.left, begin, split) &&
              derives(r.right, begin + split, len - split)) {
            ok = true;
            break;
          }
        }
        if (ok) break;
      }
    }
    m = static_cast<std::int8_t>(ok ? 1 : 0);
    return ok;
  }
};

bool oracle_accepts(const CnfGrammar& g, std::string_view input) {
  if (input.empty()) return g.start_accepts_empty;
  Oracle o{g, input,
           std::vector<std::int8_t>(input.size() * input.size() * g.nonterminal_count, -1),
           input.size()};
  return o.derives(g.start, 0, input.size());
}

TEST_CASE(cyk_deterministic_and_byte_semantics) {
  CnfGrammar g;
  g.nonterminal_count = 3;
  g.start = 0;
  g.terminal_rules = {{1, static_cast<std::uint8_t>('a')},
                      {2, static_cast<std::uint8_t>('b')}};
  g.binary_rules = {{0, 1, 2}};
  auto r = cyk_parse(g, "ab");
  REQUIRE(r.accepted && replay(g, "ab", r));
  REQUIRE(!cyk_parse(g, "aa").accepted);

  CnfGrammar bytes;
  bytes.nonterminal_count = 3;
  bytes.start = 0;
  bytes.terminal_rules = {{1, 0x00U}, {2, 0xffU}};
  bytes.binary_rules = {{0, 1, 2}};
  std::string text;
  text.push_back('\0');
  text.push_back(static_cast<char>(0xff));
  auto rb = cyk_parse(bytes, text);
  REQUIRE(rb.accepted && replay(bytes, text, rb));

  CnfGrammar empty;
  empty.nonterminal_count = 1;
  empty.start = 0;
  REQUIRE(!cyk_parse(empty, "").accepted);
  empty.start_accepts_empty = true;
  auto re = cyk_parse(empty, "");
  REQUIRE(re.accepted && replay(empty, "", re));
}

TEST_CASE(cyk_ambiguity_uses_deterministic_witness) {
  CnfGrammar g;
  g.nonterminal_count = 1;
  g.start = 0;
  g.terminal_rules = {{0, static_cast<std::uint8_t>('a')},
                      {0, static_cast<std::uint8_t>('a')}};
  g.binary_rules = {{0,0,0}, {0,0,0}};
  auto a = cyk_parse(g, "aaa");
  auto b = cyk_parse(g, "aaa");
  REQUIRE(a == b);
  REQUIRE(a.accepted && replay(g, "aaa", a));
  REQUIRE(a.root.has_value());
  REQUIRE(a.nodes[*a.root].split == 1U);
}

TEST_CASE(cyk_validates_grammar_shape) {
  CnfGrammar g;
  REQUIRE_THROWS_AS(cyk_parse(g, "a"), std::invalid_argument);
  g.nonterminal_count = 1;
  g.start = 1;
  REQUIRE_THROWS_AS(cyk_parse(g, "a"), std::out_of_range);
  g.start = 0;
  g.terminal_rules = {{1, 0U}};
  REQUIRE_THROWS_AS(cyk_parse(g, "a"), std::out_of_range);
  g.terminal_rules.clear();
  g.binary_rules = {{0,0,1}};
  REQUIRE_THROWS_AS(cyk_parse(g, "a"), std::out_of_range);
}

TEST_CASE(cyk_randomized_differential_against_top_down_oracle) {
  std::mt19937_64 rng(0xC7C7C7ULL);
  constexpr std::array<std::uint8_t, 4> alphabet{0x00U, 0x61U, 0x62U, 0xffU};
  for (int trial = 0; trial < 1200; ++trial) {
    CnfGrammar g;
    g.nonterminal_count = 1U + static_cast<std::size_t>(rng() % 5U);
    g.start = static_cast<std::size_t>(rng() % g.nonterminal_count);
    g.start_accepts_empty = (rng() % 5U) == 0U;
    for (std::size_t lhs = 0; lhs < g.nonterminal_count; ++lhs) {
      for (auto t : alphabet) {
        if ((rng() % 5U) == 0U) g.terminal_rules.push_back({lhs, t});
      }
    }
    for (std::size_t lhs = 0; lhs < g.nonterminal_count; ++lhs) {
      for (std::size_t left = 0; left < g.nonterminal_count; ++left) {
        for (std::size_t right = 0; right < g.nonterminal_count; ++right) {
          if ((rng() % 11U) == 0U) g.binary_rules.push_back({lhs, left, right});
        }
      }
    }
    const std::size_t len = static_cast<std::size_t>(rng() % 7U);
    std::string text(len, '\0');
    for (char& c : text) {
      c = static_cast<char>(alphabet[static_cast<std::size_t>(rng() % alphabet.size())]);
    }
    const bool expected = oracle_accepts(g, text);
    const auto actual = cyk_parse(g, text);
    REQUIRE(actual.accepted == expected);
    REQUIRE(replay(g, text, actual));
    REQUIRE(cyk_parse(g, text) == actual);
  }
}

}  // namespace
