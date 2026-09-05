#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
godot_bin=${GODOT_MONO_BIN:-godot-mono}
run_log=$(mktemp "${TMPDIR:-/tmp}/clay-gdextension-smoke.XXXXXX")
trap 'rm -f "$run_log"' EXIT

if [[ ! -x "$godot_bin" ]] && ! command -v "$godot_bin" >/dev/null 2>&1; then
    echo "godot mono executable not found: $godot_bin" >&2
    exit 1
fi

"$godot_bin" --headless --path "$repo_dir/integrations/godot-gdextension" \
    --quit-after 2 2>&1 | tee "$run_log"

if ! grep -q 'Clay GDExtension native node loaded: ClayRuntimeNode' "$run_log"; then
    echo "GDExtension smoke test did not instantiate ClayRuntimeNode" >&2
    exit 1
fi
if grep -Eq 'Cannot get class|WARNING: Node .*ClayRuntimeNode' "$run_log"; then
    echo "GDExtension smoke test reported a native class loading error" >&2
    exit 1
fi
echo "Godot GDExtension smoke test passed"
