#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
cd "$repo_root"

preset="windows-cpu-release"
build_dir="build/windows-cpu-release"
source_exe="$build_dir/Release/raypalette.exe"
dist_dir="dist/windows-cpu-release"
dist_exe="$dist_dir/raypalette_cpu.exe"
notices_file="THIRD_PARTY_NOTICES.txt"

cmake --preset "$preset" \
  -DBUILD_TESTING=OFF \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
  -DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF
cmake --build --preset "$preset" --target raypalette --parallel

if [[ ! -f "$source_exe" ]]; then
  echo "Build succeeded but the executable was not found: $source_exe" >&2
  exit 1
fi

rm -rf "$dist_dir"
mkdir -p "$dist_dir"
cp "$source_exe" "$dist_exe"
cp "$notices_file" "$dist_dir/"

printf 'CPU distribution executable: %s\n' "$dist_exe"