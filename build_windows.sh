#!/usr/bin/env bash
set -euo pipefail

build_type="${1:-debug}"

case "$build_type" in
	debug|release) ;;
	*)
		echo "Usage: $0 [debug|release]" >&2
		exit 2
		;;
esac

# CPU build
cmake --preset "windows-cpu-$build_type"
cmake --build --preset "windows-cpu-$build_type"
ctest --preset "windows-cpu-$build_type" --output-on-failure
./build/windows-cpu-$build_type/${build_type^}/raypalette.exe

# GPU build, when CUDA Toolkit is installed
if command -v nvcc >/dev/null 2>&1; then
	cmake --preset "windows-$build_type"
	cmake --build --preset "windows-$build_type"
	ctest --preset "windows-$build_type" --output-on-failure
	./build/windows-$build_type/${build_type^}/raypalette.exe
else
	echo "CUDA Toolkit was not found; skipping the GPU build." >&2
fi