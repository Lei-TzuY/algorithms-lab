#pragma once

TEST_CASE(earley_randomized_least_fixed_point_differential) {
  std::mt19937_64 rng(0xEA71E1ULL);
  for (std::size_t trial = 0; trial < 1000U; ++trial) {
    const std::size_t nonterminals = 1U + static_cast<std::size_t>(rng() % 4U);
    GeneralCfgGrammar grammar;
    grammar.nonterminal_count = nonterminals;
    grammar.start = static_cast<std::size_t>(rng() % nonterminals);
    const std::size_t rule_count = 1U + static_cast<std::size_t>(rng() % 9U);
    for (std::size_t r = 0; r < rule_count; ++r) {
      GeneralCfgRule rule;
      rule.lhs = static_cast<std::size_t>(rng() % nonterminals);
      const std::size_t rhs_length = static_cast<std::size_t>(rng() % 4U);
      for (std::size_t k = 0; k < rhs_length; ++k) {
        if ((rng() & 1U) == 0U) {
          rule.rhs.push_back(nt(static_cast<std::size_t>(rng() % nonterminals)));
        } else {
          rule.rhs.push_back(ch(static_cast<std::uint8_t>(rng() % 3U)));
        }
      }
      grammar.rules.push_back(std::move(rule));
      if ((rng() % 7U) == 0U) grammar.rules.push_back(grammar.rules.back());
    }
    const std::size_t input_length = static_cast<std::size_t>(rng() % 6U);
    std::string input(input_length, '\0');
    for (char& byte : input) byte = static_cast<char>(rng() % 3U);

    const bool expected = fixed_point_oracle(grammar, input);
    const EarleyParseResult first = earley_recognize(grammar, input);
    const EarleyParseResult second = earley_recognize(grammar, input);
    REQUIRE_EQ(first.accepted, expected);
    REQUIRE_EQ(first, second);
    REQUIRE_EQ(first.chart_item_counts.size(), input.size() + 1U);
  }
}


TEST_CASE(earley_matches_sealed_cyk_on_cnf_subdomain) {
  CnfGrammar empty_cnf{1U, 0U, true, {}, {}};
  GeneralCfgGrammar empty_general{1U, 0U, {{0U, {}}}};
  REQUIRE_EQ(earley_recognize(empty_general, "").accepted,
             cyk_parse(empty_cnf, "").accepted);

  std::mt19937_64 rng(0xC1C0EA71ULL);
  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t nonterminals = 1U + static_cast<std::size_t>(rng() % 4U);
    CnfGrammar cnf;
    cnf.nonterminal_count = nonterminals;
    cnf.start = static_cast<std::size_t>(rng() % nonterminals);
    cnf.start_accepts_empty = false;
    const std::size_t terminal_rules = static_cast<std::size_t>(rng() % 9U);
    const std::size_t binary_rules = static_cast<std::size_t>(rng() % 9U);
    for (std::size_t i = 0; i < terminal_rules; ++i) {
      cnf.terminal_rules.push_back(CnfTerminalRule{
          static_cast<std::size_t>(rng() % nonterminals),
          static_cast<std::uint8_t>(rng() % 3U)});
    }
    for (std::size_t i = 0; i < binary_rules; ++i) {
      cnf.binary_rules.push_back(CnfBinaryRule{
          static_cast<std::size_t>(rng() % nonterminals),
          static_cast<std::size_t>(rng() % nonterminals),
          static_cast<std::size_t>(rng() % nonterminals)});
    }

    GeneralCfgGrammar general;
    general.nonterminal_count = nonterminals;
    general.start = cnf.start;
    for (const CnfTerminalRule& rule : cnf.terminal_rules) {
      general.rules.push_back(GeneralCfgRule{rule.lhs, {ch(rule.terminal)}});
    }
    for (const CnfBinaryRule& rule : cnf.binary_rules) {
      general.rules.push_back(GeneralCfgRule{rule.lhs, {nt(rule.left), nt(rule.right)}});
    }

    const std::size_t input_length = static_cast<std::size_t>(rng() % 6U);
    std::string input(input_length, '\0');
    for (char& byte : input) byte = static_cast<char>(rng() % 3U);
    REQUIRE_EQ(earley_recognize(general, input).accepted, cyk_parse(cnf, input).accepted);
  }
}
