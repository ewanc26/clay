# Open issues

## GitHub API unavailable during integration audit

On 2026-09-02, `gh issue list` could not connect to `api.github.com`, so
remote issue filing could not be completed from this environment. Re-run the
command when network access is available.

## Godot Mono binding package

Clay still needs generated key constants, native library packaging for each
supported platform, and a CI smoke project. A minimal sample project, managed
`SafeHandle` wrapper, and configurable shared-library build now exist under
`integrations/godot-mono/`; this issue remains open until those distribution
pieces are covered.

## Windows and Godot export validation

The shared C ABI now uses an explicit Windows export macro, but the repository
still needs a Windows CI job and an exported Godot Mono sample run to validate
DLL naming, dependent runtime loading, and architecture selection.

## Godot GDExtension adapter

The current Mono integration uses a stable C ABI and P/Invoke. It does not
provide a Godot entry symbol or `.gdextension` manifest, so it cannot be
consumed as a first-class GDExtension. Decide whether to maintain a separate
GDExtension adapter after the Mono sample is validated.
