#!/usr/bin/env bash
set -euo pipefail

output_path=${1:?usage: godot_mono_stage_native.sh EXPORTED_PLAYER NATIVE_LIBRARY}
native_lib=${2:?usage: godot_mono_stage_native.sh EXPORTED_PLAYER NATIVE_LIBRARY}

if [[ ! -f "$native_lib" ]]; then
    echo "Clay native library not found: $native_lib" >&2
    exit 1
fi

if [[ "$output_path" == *.app ]]; then
    destination="$output_path/Contents/MacOS"
else
    destination=$(dirname "$output_path")
fi

mkdir -p "$destination"
cp "$native_lib" "$destination/"
echo "Staged Clay native library: $destination/$(basename "$native_lib")"
