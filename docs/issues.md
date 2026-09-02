# Open issues

## GitHub API unavailable during integration audit

On 2026-09-02, `gh issue list` could not connect to `api.github.com`, so
remote issue filing could not be completed from this environment. Re-run the
command when network access is available.

## Godot Mono binding package

Clay still needs a maintained Godot Mono binding layer with generated managed
types, native handle ownership, and a CI smoke project. The installed C ABI is
now suitable as the native boundary, but this integration remains open.
