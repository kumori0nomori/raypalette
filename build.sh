#
# library
#

# GPU
cmake --preset linux-debug
cmake --build --preset linux-debug --target format
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure

# CPU
cmake --preset linux-cpu-debug
cmake --build --preset linux-cpu-debug --target format
cmake --build --preset linux-cpu-debug
ctest --preset linux-cpu-debug --output-on-failure

#
# GUI
#

# CPU-only build (does not require CUDA Toolkit or an NVIDIA GPU)
# cmake --preset linux-cpu-debug
# cmake --build --preset linux-cpu-debug --target format
# ctest --preset linux-cpu-debug --output-on-failure

cmake --preset linux-gui-debug
cmake --build --preset linux-gui-debug --target format
cmake --build --preset linux-gui-debug
ctest --preset linux-gui-debug --output-on-failure
./build/linux-gui-debug/raypalette_gui
