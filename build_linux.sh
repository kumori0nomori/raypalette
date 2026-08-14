#!/usr/bin/env bash
set -euo pipefail

build_type="${1:-release}"

case "$build_type" in
  debug|release) ;;
  *)
    echo "Usage: $0 [debug|release]" >&2
    exit 2
    ;;
esac

# CPU-only build (does not require CUDA Toolkit or an NVIDIA GPU)
cmake --preset linux-cpu-$build_type
cmake --build --preset linux-cpu-$build_type --target format
cmake --build --preset linux-cpu-$build_type
ctest --preset linux-cpu-$build_type --output-on-failure
./build/linux-cpu-$build_type/raypalette_gui

# CUDA build (requires CUDA Toolkit and an NVIDIA GPU)
cmake --preset linux-$build_type
cmake --build --preset linux-$build_type --target format
cmake --build --preset linux-$build_type
ctest --preset linux-$build_type --output-on-failure
./build/linux-$build_type/raypalette_gui
