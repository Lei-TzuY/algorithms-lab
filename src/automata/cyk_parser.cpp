#include "algorithms/automata/cyk_parser.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace algorithms::automata {
namespace {

enum class BackKind : std::uint8_t { None, Terminal, Binary };

struct Backpointer {
  BackKind kind = BackKind::None;
  std::size_t split = 0;
  CfgNonterminal left = 0;
  CfgNonterminal right = 0;
  std::uint8_t terminal = 0;
};

struct NormalizedGrammar {
  std::size_t nonterminal_count = 0;
  CfgNonterminal start = 0;
  bool start_accepts_empty = false;
  std::vector<CnfTerminalRule> terminal_rules;
  std::vector<CnfBinaryRule> binary_rules;
};

NormalizedGrammar normalize(const CnfGrammar& grammar) {
  if (grammar.nonterminal_count == 0) {
    throw std::invalid_argument("CYK grammar must contain a nonterminal");
  }
  if (grammar.start >= grammar.nonterminal_count) {
    throw std::out_of_range("CYK start nonterminal is out of range");
  }

  NormalizedGrammar result;
  result.nonterminal_count = grammar.nonterminal_count;
  result.start = grammar.start;
  result.start_accepts_empty = grammar.start_accepts_empty;
  result.terminal_rules = grammar.terminal_rules;
  result.binary_rules = grammar.binary_rules;

  for (const CnfTerminalRule& rule : result.terminal_rules) {
    if (rule.lhs >= result.nonterminal_count) {
      throw std::out_of_range("CYK terminal production has invalid nonterminal");
    }
  }
  for (const CnfBinaryRule& rule : result.binary_rules) {
    if (rule.lhs >= result.nonterminal_count ||
        rule.left >= result.nonterminal_count ||
        rule.right >= result.nonterminal_count) {
      throw std::out_of_range("CYK binary production has invalid nonterminal");
    }
  }

  std::sort(result.terminal_rules.begin(), result.terminal_rules.end(),
            [](const CnfTerminalRule& a, const CnfTerminalRule& b) {
              return std::tie(a.lhs, a.terminal) < std::tie(b.lhs, b.terminal);
            });
  result.terminal_rules.erase(
      std::unique(result.terminal_rules.begin(), result.terminal_rules.end()),
      result.terminal_rules.end());

  std::sort(result.binary_rules.begin(), result.binary_rules.end(),
            [](const CnfBinaryRule& a, const CnfBinaryRule& b) {
              return std::tie(a.lhs, a.left, a.right) <
                     std::tie(b.lhs, b.left, b.right);
            });
  result.binary_rules.erase(
      std::unique(result.binary_rules.begin(), result.binary_rules.end()),
      result.binary_rules.end());
  return result;
}

std::size_t checked_state_count(std::size_t n, std::size_t nonterminals) {
  if (n == 0) return 0;
  const std::size_t max = std::numeric_limits<std::size_t>::max();
  if (n > max / n) {
    throw std::length_error("CYK chart dimensions overflow size_t");
  }
  const std::size_t cells = n * n;
  if (nonterminals > max / cells) {
    throw std::length_error("CYK chart state count overflows size_t");
  }
  return cells * nonterminals;
}

class Chart {
 public:
  Chart(std::size_t n, std::size_t nonterminals)
      : n_(n), nonterminals_(nonterminals),
        reachable_(checked_state_count(n, nonterminals), 0U),
        back_(reachable_.size()) {}

  [[nodiscard]] bool get(std::size_t begin, std::size_t length,
                         CfgNonterminal nonterminal) const {
    return reachable_[index(begin, length, nonterminal)] != 0U;
  }

  Backpointer& back(std::size_t begin, std::size_t length,
                    CfgNonterminal nonterminal) {
    return back_[index(begin, length, nonterminal)];
  }

  const Backpointer& back(std::size_t begin, std::size_t length,
                          CfgNonterminal nonterminal) const {
    return back_[index(begin, length, nonterminal)];
  }

  void set(std::size_t begin, std::size_t length,
           CfgNonterminal nonterminal, Backpointer pointer) {
    const std::size_t state = index(begin, length, nonterminal);
    reachable_[state] = 1U;
    back_[state] = pointer;
  }

 private:
  [[nodiscard]] std::size_t index(std::size_t begin, std::size_t length,
                                  CfgNonterminal nonterminal) const {
    return ((begin * n_) + (length - 1U)) * nonterminals_ + nonterminal;
  }

  std::size_t n_;
  std::size_t nonterminals_;
  std::vector<std::uint8_t> reachable_;
  std::vector<Backpointer> back_;
};

std::size_t build_witness(const Chart& chart, CfgNonterminal nonterminal,
                          std::size_t begin, std::size_t length,
                          std::vector<CykParseNode>& nodes) {
  const Backpointer& pointer = chart.back(begin, length, nonterminal);
  if (pointer.kind == BackKind::Terminal) {
    const std::size_t node_index = nodes.size();
    nodes.push_back(CykParseNode{nonterminal, begin, begin + 1U,
                                 CykNodeKind::Terminal, pointer.terminal,
                                 0U, 0U, 0U});
    return node_index;
  }
  if (pointer.kind != BackKind::Binary || pointer.split == 0U ||
      pointer.split >= length) {
    throw std::logic_error("CYK accepted state has no valid backpointer");
  }

  const std::size_t left =
      build_witness(chart, pointer.left, begin, pointer.split, nodes);
  const std::size_t right =
      build_witness(chart, pointer.right, begin + pointer.split,
                    length - pointer.split, nodes);
  const std::size_t node_index = nodes.size();
  nodes.push_back(CykParseNode{nonterminal, begin, begin + length,
                               CykNodeKind::Binary, 0U,
                               begin + pointer.split, left, right});
  return node_index;
}

}  // namespace

CykParseResult cyk_parse(const CnfGrammar& grammar, std::string_view input) {
  const NormalizedGrammar normalized = normalize(grammar);
  CykParseResult result;
  if (input.empty()) {
    result.accepted = normalized.start_accepts_empty;
    return result;
  }

  const std::size_t n = input.size();
  Chart chart(n, normalized.nonterminal_count);

  for (std::size_t position = 0; position < n; ++position) {
    const std::uint8_t terminal = static_cast<std::uint8_t>(
        static_cast<unsigned char>(input[position]));
    for (const CnfTerminalRule& rule : normalized.terminal_rules) {
      if (rule.terminal != terminal || chart.get(position, 1U, rule.lhs)) {
        continue;
      }
      chart.set(position, 1U, rule.lhs,
                Backpointer{BackKind::Terminal, 0U, 0U, 0U, terminal});
    }
  }

  for (std::size_t length = 2; length <= n; ++length) {
    for (std::size_t begin = 0; begin + length <= n; ++begin) {
      for (std::size_t split = 1; split < length; ++split) {
        for (const CnfBinaryRule& rule : normalized.binary_rules) {
          if (chart.get(begin, length, rule.lhs)) continue;
          if (!chart.get(begin, split, rule.left)) continue;
          if (!chart.get(begin + split, length - split, rule.right)) continue;
          chart.set(begin, length, rule.lhs,
                    Backpointer{BackKind::Binary, split, rule.left,
                                rule.right, 0U});
        }
      }
    }
  }

  if (!chart.get(0U, n, normalized.start)) return result;
  result.accepted = true;
  result.root = build_witness(chart, normalized.start, 0U, n, result.nodes);
  return result;
}

}  // namespace algorithms::automata
