# Open issues

## GitHub API unavailable during integration audit

On 2026-09-02, `gh issue list` could not connect to `api.github.com`, so
remote issue filing could not be completed from this environment. Re-run the
command when network access is available.

## Godot Mono binding package

Clay still needs a complete Godot sample project, generated key constants,
native library packaging for each supported platform, and a CI smoke project.
The initial managed `SafeHandle` wrapper now exists under
`integrations/godot-mono/`; this issue remains open until those distribution
pieces are covered.
