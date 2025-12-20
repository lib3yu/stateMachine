# Repository Guidelines

## Project Structure & Module Organization
- `src/` hosts the FSM runtime and Doxygen-annotated API (`stateMachine.c` / `stateMachine.h`); keep header comments synchronized with implementation changes.
- `examples/` contains runnable demos (`stateMachineExample.c`, `loginStateMachineExample.c`, `motorCtrlStateMachine.c`) plus the `queue.*` helper; builds emit binaries into `bin/`.
- `tests/nestedTest.c` is the canonical hierarchical regression—use it as the template for new suites and place extra fixtures alongside the test file.
- `doc/` stores `doxyconfig` and image assets under `doc/img/`; regenerate docs whenever exported symbols or diagrams change.

## Build, Test, and Development Commands
- `make` (default target) runs `clean` then `dist`, giving every example binary from a fresh tree.
- `make dist` alone rebuilds `bin/example`, `bin/login_example`, and `bin/motor_example` without wiping `bin/`.
- `make test && ./bin/test` compiles `tests/nestedTest.c` against the library and executes it; extend the Makefile target list as more tests land.
- `make clean` removes `bin/`; run before benchmarking or packaging to avoid stale artifacts.
- `doxygen doc/doxyconfig` refreshes the published API reference.

## Coding Style & Naming Conventions
- Use C99 with space-indented, Allman-style braces as shown in `src/stateMachine.c`; keep pointer spacing (`type *ptr`) consistent.
- Public APIs carry the `stateM_` prefix in the header; helper functions remain `static` in the `.c` file.
- Structs (`state`, `event`, `transition`) use lowercase identifiers, enum constants use `Event_*`, and inline transition arrays keep guards/actions visible above the usage site.
- Document complex flows with block comments or ASCII diagrams similar to the one in `tests/nestedTest.c`.

## Testing Guidelines
- Build transition trees directly inside each test so guards, entry/exit actions, and error-state handling remain observable without fixtures.
- Name tests `<feature>Test.c`, keep them standalone, and update the Makefile so `make test` exercises every scenario.
- Tests should return non-zero on failure and minimize stdout noise for CI; prefer deterministic payloads over random data.

## Commit & Pull Request Guidelines
- Follow the `<scope>: <summary>` commit style already in the log (`examples: add motor control demo`, `fix: bubble parent transitions`); scopes usually map to top-level directories.
- PRs must describe observable behavior, call out impacted binaries/tests/docs, and attach sample output or screenshots when examples or diagrams change.
- Include the commands you ran (`make`, `make test`, `doxygen …`) and link related issues so reviewers can trace the change quickly.
