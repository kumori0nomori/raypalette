#!/usr/bin/env bash
set -euo pipefail

preset="windows-cpu-release"
build_dir="build/windows-cpu-release"
source_exe="$build_dir/Release/raypalette.exe"
dist_dir="dist/windows-cpu-release"
dist_exe="$dist_dir/raypalette_cpu.exe"

cmake --preset "$preset" \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
  -DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF
cmake --build --preset "$preset" --target raypalette --parallel
ctest --preset "$preset" --output-on-failure

if [[ ! -f "$source_exe" ]]; then
  echo "Build succeeded but the executable was not found: $source_exe" >&2
  exit 1
fi

rm -rf "$dist_dir"
mkdir -p "$dist_dir"
cp "$source_exe" "$dist_exe"

printf 'CPU distribution executable: %s\n' "$dist_exe"
