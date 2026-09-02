#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
godot_bin=${GODOT_MONO_BIN:-godot-mono}
native_lib=${CLAY_NATIVE_LIBRARY:-$repo_dir/build/libclay_engine.dylib}
output_path=${1:-$repo_dir/build/ClayGodotSample.app}

if [[ ! -f "$native_lib" ]]; then
    echo "Clay native library not found: $native_lib" >&2
    exit 1
fi
if [[ ! -x "$godot_bin" ]] && ! command -v "$godot_bin" >/dev/null 2>&1; then
    echo "godot mono executable not found: $godot_bin" >&2
    exit 1
fi

dotnet build "$repo_dir/integrations/godot-mono/ClayGodotSample.csproj" \
    --nologo --ignore-failed-sources
"$godot_bin" --headless --path "$repo_dir/integrations/godot-mono" \
    --editor --build-solutions --quit-after 10
"$godot_bin" --headless --path "$repo_dir/integrations/godot-mono" \
    --export-debug macOS "$output_path"

mkdir -p "$output_path/Contents/MacOS"
cp "$native_lib" "$output_path/Contents/MacOS/"
echo "Exported Clay Godot Mono sample: $output_path"
