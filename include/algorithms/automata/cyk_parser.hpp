#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace algorithms::automata {

using CfgNonterminal = std::size_t;

struct CnfTerminalRule {
  CfgNonterminal lhs;
  std::uint8_t terminal;

  friend bool operator==(const CnfTerminalRule&, const CnfTerminalRule&) = default;
};

struct CnfBinaryRule {
  CfgNonterminal lhs;
  CfgNonterminal left;
  CfgNonterminal right;

  friend bool operator==(const CnfBinaryRule&, const CnfBinaryRule&) = default;
};

struct CnfGrammar {
  std::size_t nonterminal_count = 0;
  CfgNonterminal start = 0;
  bool start_accepts_empty = false;
  std::vector<CnfTerminalRule> terminal_rules;
  std::vector<CnfBinaryRule> binary_rules;
};

enum class CykNodeKind : std::uint8_t {
  Terminal,
  Binary,
};

struct CykParseNode {
  CfgNonterminal nonterminal = 0;
  std::size_t begin = 0;
  std::size_t end = 0;
  CykNodeKind kind = CykNodeKind::Terminal;
  std::uint8_t terminal = 0;
  std::size_t split = 0;
  std::size_t left_child = 0;
  std::size_t right_child = 0;

  friend bool operator==(const CykParseNode&, const CykParseNode&) = default;
};

struct CykParseResult {
  bool accepted = false;
  std::vector<CykParseNode> nodes;
  std::optional<std::size_t> root;

  friend bool operator==(const CykParseResult&, const CykParseResult&) = default;
};

// Recognize a byte string with a grammar in Chomsky normal form. The grammar
// representation admits only A->byte and A->BC productions; the optional empty
// word is represented explicitly by start_accepts_empty.
//
// Duplicate productions are accepted and canonicalized internally. For an
// ambiguous non-empty input, the witness is deterministic: the parser chooses
// the smallest split first, then the lexicographically smallest (lhs,left,right)
// binary production compatible with that split. Terminal ties choose the
// smallest lhs after rule canonicalization.
//
// Direct educational baseline: O(n * Rt + n^3 * Rb) time and
// O(n^2 * N) state for input length n, Rt terminal productions, Rb binary
// productions, and N nonterminals.
[[nodiscard]] CykParseResult cyk_parse(const CnfGrammar& grammar,
                                       std::string_view input);

}  // namespace algorithms::automata
