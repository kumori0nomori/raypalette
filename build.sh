#
# GUI
#

# CPU-only build (does not require CUDA Toolkit or an NVIDIA GPU)
cmake --preset linux-cpu-gui-debug
cmake --build --preset linux-cpu-gui-debug --target format
cmake --build --preset linux-cpu-gui-debug
ctest --preset linux-cpu-gui-debug --output-on-failure
./build/linux-cpu-gui-debug/raypalette_gui

cmake --preset linux-gui-debug
cmake --build --preset linux-gui-debug --target format
cmake --build --preset linux-gui-debug
ctest --preset linux-gui-debug --output-on-failure
./build/linux-gui-debug/raypalette_gui
