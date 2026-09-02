#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
godot_bin=${GODOT_MONO_BIN:-godot-mono}
native_lib=${CLAY_NATIVE_LIBRARY:-"$repo_dir/build/libclay_engine.dylib"}
stage_dir=$(mktemp -d "${TMPDIR:-/tmp}/clay-godot-smoke.XXXXXX")
trap 'rm -rf "$stage_dir"' EXIT

if [[ ! -x "$godot_bin" ]] && ! command -v "$godot_bin" >/dev/null 2>&1; then
    echo "godot mono executable not found: $godot_bin" >&2
    exit 1
fi
if [[ ! -f "$native_lib" ]]; then
    echo "Clay native library not found: $native_lib" >&2
    exit 1
fi

cp "$repo_dir/integrations/godot-mono/ClayDemo.cs" \
   "$repo_dir/integrations/godot-mono/ClayDemo.tscn" \
   "$repo_dir/integrations/godot-mono/ClayGodotSample.csproj" \
   "$repo_dir/integrations/godot-mono/ClayKey.cs" \
   "$repo_dir/integrations/godot-mono/ClayModifiers.cs" \
   "$repo_dir/integrations/godot-mono/ClayRuntime.cs" \
   "$repo_dir/integrations/godot-mono/project.godot" "$stage_dir/"
cp "$native_lib" "$stage_dir/"

dotnet build "$stage_dir/ClayGodotSample.csproj" --nologo --ignore-failed-sources
"$godot_bin" --headless --path "$stage_dir" --editor \
    --build-solutions --quit-after 10
run_log="$stage_dir/game.log"
"$godot_bin" --headless --path "$stage_dir" --quit-after 30 \
    2>&1 | tee "$run_log"
if grep -Eq 'Cannot instantiate C# script|Cannot load|Unable to load|DllNotFoundException|EntryPointNotFoundException|Clay runtime creation failed|Clay error:' "$run_log"; then
    echo "Godot Mono smoke test reported a runtime loading error" >&2
    exit 1
fi
echo "Godot Mono smoke test passed"
