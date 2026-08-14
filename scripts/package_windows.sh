#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
cd "$repo_root"

preset="windows-release"
build_dir="build/windows-release"
source_exe="$build_dir/Release/raypalette.exe"
dist_dir="dist/windows-release"
dist_exe="$dist_dir/raypalette.exe"
notices_file="THIRD_PARTY_NOTICES.txt"
sha256sums_file="$dist_dir/SHA256SUMS.txt"

if ! command -v nvcc >/dev/null 2>&1; then
  echo "CUDA Toolkit was not found. Install CUDA Toolkit before packaging the GPU build." >&2
  exit 1
fi

cmake --preset "$preset" \
  -DBUILD_TESTING=OFF \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
  -DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF \
  -DCMAKE_CUDA_RUNTIME_LIBRARY=Static
cmake --build --preset "$preset" --target raypalette --parallel

if [[ ! -f "$source_exe" ]]; then
  echo "Build succeeded but the executable was not found: $source_exe" >&2
  exit 1
fi

rm -rf "$dist_dir"
mkdir -p "$dist_dir"
cp "$source_exe" "$dist_exe"
cp "$notices_file" "$dist_dir/"
sha256sum "$dist_exe" > "$sha256sums_file"

printf 'GPU distribution executable: %s\n' "$dist_exe"
printf 'Third-party notices file: %s\n' "$dist_dir/$notices_file"
printf 'SHA256SUMS file: %s\n' "$sha256sums_file"