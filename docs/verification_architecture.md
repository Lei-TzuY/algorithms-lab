# Verification architecture hardening

## Motivation

The post-Phase-69 recovery surface now contains many independently useful test suites. Historically every `tests/test_*_cases.hpp` suite was included directly by `tests/test_main.cpp`, producing one mega translation unit. That arrangement couples otherwise unrelated private implementation details at compile time. PR #340 exposed this concretely when two unrelated headers used the same `algorithms::graphs::detail` helper namespace: focused builds passed, but the full-repository build failed only because both headers were textually included into the same translation unit.

This checkpoint changes verification architecture rather than adding another algorithm name.

## Isolated recovery-suite compilation

Every source-tree header matching `tests/test_*_cases.hpp` is treated as one self-registering recovery test suite. CMake discovers these headers with `CONFIGURE_DEPENDS` and generates one tiny wrapper source per header in the build tree. Each wrapper first includes the shared `test_framework.hpp` registration/assertion contract and then includes exactly one case header.

The wrappers are linked into the existing `algorithms_tests` executable together with the ordinary `tests/test_*.cpp` sources. The public test execution model stays one executable and one full-suite CTest gate, but each recovery suite now compiles in its own translation unit with only the shared test framework prelude. Private helper names and transitive includes therefore cannot leak between unrelated recovery suites, while historical case headers do not each need to duplicate the common framework include.

The filename convention is part of the verification contract: a new recovery suite named `test_<surface>_cases.hpp` is auto-enrolled after CMake reconfiguration. No generated wrapper is written into the source tree.

## Deterministic registry contract

Static registration order across translation units is unspecified, so the runner copies the registry and sorts test cases lexicographically by name before listing or execution. Duplicate test names are rejected before any test runs. This preserves one globally addressable identity per test while making output independent of linker/static-initialization order.

Default invocation still executes every registered test. `--list` lists the selected tests without executing them, and `--filter <substring>` (or `--filter=<substring>`) selects a deterministic subset for focused local replay. A filter that matches nothing fails rather than silently passing. These focused controls do not reduce the authoritative CI contract: GitHub CI continues to invoke the unfiltered full suite.

## Evidence and non-claims

The full GCC release, Clang release, and GCC ASan+UBSan CI matrix remains the integration authority. Successful compilation under this layout is evidence that every recovery suite is isolated from neighboring case headers and that the shared test framework prelude is sufficient for registration/assertion macros. Full-suite execution verifies that cross-translation-unit registration still discovers and runs the suites.

This checkpoint makes no clean-build speedup claim. Splitting a mega translation unit trades repeated parsing/link inputs for compile isolation and finer incremental rebuild boundaries. It also does not introduce a property-testing framework, randomized-test scheduler, or CI sharding policy; those would require separate evidence if they become real bottlenecks.
