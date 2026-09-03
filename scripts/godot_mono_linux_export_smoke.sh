#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
project_dir="$repo_dir/integrations/godot-mono"
godot_bin=${GODOT_MONO_BIN:-godot-mono}
native_lib=${CLAY_NATIVE_LIBRARY:-}

if [[ -z "$native_lib" ]]; then
    for candidate in \
        "$repo_dir/build/libclay_engine.so" \
        "$repo_dir/build/Debug/libclay_engine.so"; do
        if [[ -f "$candidate" ]]; then
            native_lib=$candidate
            break
        fi
    done
fi

if [[ ! -x "$godot_bin" ]] && ! command -v "$godot_bin" >/dev/null 2>&1; then
    echo "Godot .NET editor not found: $godot_bin" >&2
    exit 1
fi
if [[ ! -f "$native_lib" ]]; then
    echo "Clay native library not found: $native_lib" >&2
    exit 1
fi

stage_dir=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/clay-godot-linux-export.XXXXXX")
trap 'rm -rf "$stage_dir"' EXIT
export_dir="$stage_dir/export"
mkdir -p "$export_dir"

dotnet build "$project_dir/ClayGodotSample.csproj" --nologo --ignore-failed-sources

editor_log="$stage_dir/editor.log"
"$godot_bin" --headless --path "$project_dir" --editor \
    --build-solutions --quit-after 30 2>&1 | tee "$editor_log"

export_path="$export_dir/ClayGodotSample.x86_64"
export_log="$stage_dir/export.log"
"$godot_bin" --headless --path "$project_dir" \
    --export-debug "Linux/X11" "$export_path" 2>&1 | tee "$export_log"
if [[ ! -x "$export_path" ]]; then
    echo "Godot did not produce executable $export_path" >&2
    exit 1
fi

cp "$native_lib" "$export_dir/libclay_engine.so"

run_log="$stage_dir/player.log"
LD_LIBRARY_PATH="$export_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$export_path" --headless --quit-after 30 2>&1 | tee "$run_log"

errors='Cannot instantiate C# script|Cannot load|Unable to load|DllNotFoundException|EntryPointNotFoundException|BadImageFormatException|Clay runtime creation failed|Clay error:'
if grep -Eq "$errors" "$run_log"; then
    echo "Exported Godot player reported a native/managed load error" >&2
    exit 1
fi
if ! grep -Fq 'Clay Godot sample rendered native frame' "$run_log"; then
    echo "Exported Godot player did not render a frame through Clay" >&2
    exit 1
fi

echo "Godot Linux exported-player smoke test passed"
