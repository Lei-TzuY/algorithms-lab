# Scope recovery: byte regular-expression derivatives

## Coverage decision

After symmetric Lanczos tridiagonalization merged as
`main@2565542fa3ff0eb8400e3399eb5c5eb005f17f45`, a fresh live-state audit
found zero open pull requests and zero open issues.

The repository already contains epsilon-NFA subset construction and Hopcroft DFA
minimization. It did not contain a regular-expression representation,
Brzozowski derivative implementation, regex matcher, regex branch, or prior
regex implementation pull request.

This slice therefore adds a distinct formal-language capability: direct exact
language membership by repeated left quotients of an immutable regular-
expression AST. It does not compile through the existing NFA/DFA path.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the historical compiler/backend
frontier remains frozen.

## Public model

`ByteRegex` is an immutable explicit AST over arbitrary bytes with exactly six
constructors:

- empty language `∅`;
- epsilon `ε`;
- one byte literal;
- alternation `R | S`;
- concatenation `RS`;
- Kleene star `R*`.

There is intentionally no textual regex parser in this slice. Escaping,
precedence, character classes, anchors, captures, Unicode policy, and syntax
errors are separate concerns.

`matches(input)` is **full-string** language membership, not substring search.

The byte conversion from `std::string_view` goes through `unsigned char`,
so embedded NUL and bytes 128..255 are ordinary alphabet values rather than
sign-extended characters.

## Nullability

Production computes whether epsilon belongs to a language by the standard
structural rules:

- `nullable(∅) = false`;
- `nullable(ε) = true`;
- literals are not nullable;
- `nullable(R|S) = nullable(R) || nullable(S)`;
- `nullable(RS) = nullable(R) && nullable(S)`;
- `nullable(R*) = true`.

After all input bytes are consumed, the current residual expression accepts
exactly when it is nullable.

## Brzozowski derivative recurrence

For a byte `a`, `D_a(R)` denotes the left quotient of language `R` by
`a`: all suffixes `w` such that `aw` belongs to `R`.

Production implements:

- `D_a(∅) = ∅`;
- `D_a(ε) = ∅`;
- `D_a(b) = ε` when `a=b`, otherwise `∅`;
- `D_a(R|S) = D_a(R) | D_a(S)`;
- `D_a(RS) = D_a(R)S | D_a(S)` when `R` is nullable;
- otherwise `D_a(RS) = D_a(R)S`;
- `D_a(R*) = D_a(R)R*`.

Matching repeatedly replaces the current expression by its derivative for the
next byte. By induction on the consumed prefix, the current expression denotes
the exact residual language after that prefix. Nullability after the final byte
therefore decides exact full-string membership.

## Safe smart-constructor simplification

To limit obvious derivative growth without claiming canonical minimization,
construction applies only identities that are language equalities:

- `∅ | R = R`;
- `R | R = R` when the two ASTs are structurally equal;
- `∅R = R∅ = ∅`;
- `εR = Rε = R`;
- `∅* = ε`;
- `ε* = ε`;
- `(R*)* = R*`.

Alternation is not globally flattened, sorted, or algebraically minimized.
Equivalent expressions with different AST shapes may remain distinct.

The implementation therefore does **not** claim a canonical regex form or a
minimal derivative automaton.

## Independent verification

The committed primary oracle does not call derivatives, nullability, NFA
determinization, or DFA minimization.

Tests build a separate test-only regex specification tree. The oracle directly
computes the set of input positions reachable after matching that specification
from a given starting position:

- epsilon returns the same position;
- a literal advances by one matching byte;
- alternation unions ending-position sets;
- concatenation feeds every left ending position into the right expression;
- star performs a finite fixed-point closure over input positions.

Because an input of length `n` has only `n+1` positions, star closure
terminates even when its operand is nullable.

Committed evidence includes:

- empty, epsilon, literal, alternation, concatenation, and star semantics;
- smart-constructor identities;
- a fixed nullable-prefix concatenation case that exercises the second term of
  the concatenation derivative recurrence;
- a known suffix language `(a|b)*abb`;
- embedded byte `0x00` and byte `0xff`;
- direct left-quotient identity checks;
- 320 fixed-seed random regex specifications of depth at most three;
- every word of length zero through four over a three-byte alphabet, for
  38,720 random regex/word oracle comparisons;
- derivative-prefix equivalence for every non-empty generated word.

A supplementary model-level differential pass used the same mathematical
position-set characterization but an independent implementation and compared
250,000 additional random regex/input pairs. It found zero mismatch.

Those model checks are supporting evidence only. Repository GCC, Clang, and
sanitizer CI remain the integration gate.

## Complexity boundary

Let `|R|` be the current AST size and `n` the input length.

One derivative recursively visits the current AST and may allocate a new AST.
The smart constructors remove only a small set of local identities. In the
worst case, repeated derivatives can grow the expression substantially; this
slice intentionally makes no polynomial AST-size or matching-time claim.

`node_count()` is a diagnostic tree-occurrence count and is not a unique
shared-node count.

The representation uses immutable `shared_ptr<const Node>` nodes, so
unchanged subexpressions can be shared by derivatives and smart constructors.

## Non-claims

This slice does not claim:

- a textual regex parser;
- substring search or find-all semantics;
- captures, backreferences, lookaround, anchors, or non-regular extensions;
- character classes or Unicode semantics;
- Thompson construction;
- derivative-state hash-consing or memoization;
- canonical algebraic regex normalization;
- minimal DFA construction;
- bounded derivative-state count;
- ReDoS-style complexity guarantees;
- benchmark-backed performance.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/automata/regex_derivative.hpp`;
- `tests/test_regex_derivative_cases.hpp`;
- `docs/scope_recovery_regex_derivative.md`.

Pre-PR hardening includes:

- defining the nested regex node type before inline smart constructors instantiate
  `make_shared`;
- a deterministic nullable-prefix concatenation regression.

No CMake, README, ROADMAP, workflow, benchmark, parser, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `2565542fa3ff0eb8400e3399eb5c5eb005f17f45`.
