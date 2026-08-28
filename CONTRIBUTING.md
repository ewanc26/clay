# Contributing to Clay

Clay follows the account's native-repo conventions (see `AGENTS.md` and
`keepsake`/`wolfram` for the same rules):

- Atomic conventional commits (`feat(core)`, `fix(render)`, ...), feature
  branches merged to `main` with `--no-ff`, no pull request without being
  asked.
- Every change ships with tests where it touches logic. Tests run through
  `ctest` and must stay headless.
- The C/C++ divide (`src/core` = C23, everywhere else = C++23) is final.
  Proposals to blur it should be an RFC in `docs/`, not a commit.
- Format with `.clang-format`.

## Getting started

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Then read `docs/architecture.md` (once it exists) and `AGENTS.md`.

## Reporting bugs

Open an issue with: the platform, the build flags, and whether the bug
reproduces headless (`clay_player --headless`) or only in the GLFW app. A
`.clayrec` recording plus `--replay` steps is ideal, because Clay can replay
its own history.