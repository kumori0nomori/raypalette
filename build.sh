#
# library
#

# GPU
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
./build/linux-debug/raypalette

# CPU
cmake --preset linux-cpu-debug
cmake --build --preset linux-cpu-debug
ctest --preset linux-cpu-debug --output-on-failure
./build/linux-cpu-debug/raypalette

#
# GUI
#

# CPU-only build (does not require CUDA Toolkit or an NVIDIA GPU)
# cmake --preset linux-cpu-debug
# cmake --build --preset linux-cpu-debug
# ctest --preset linux-cpu-debug --output-on-failure
# ./build/linux-cpu-debug/raypalette

# cmake --preset linux-gui-debug
# cmake --build --preset linux-gui-debug
# ctest --preset linux-gui-debug --output-on-failure
# ./build/linux-gui-debug/raypalette_gui

#
# Release build (requires CUDA Toolkit and an NVIDIA GPU)
#

# cmake --preset linux-release
# cmake --build --preset linux-release
# ctest --preset linux-release --output-on-failure
# ./build/linux-release/raypalette

# cmake --preset linux-gui-release
# cmake --build --preset linux-gui-release
# ctest --preset linux-gui-release --output-on-failure
# ./build/linux-gui-release/raypalette_gui