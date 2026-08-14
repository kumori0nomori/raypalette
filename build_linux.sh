# CPU-only build (does not require CUDA Toolkit or an NVIDIA GPU)
cmake --preset linux-cpu-debug
cmake --build --preset linux-cpu-debug --target format
cmake --build --preset linux-cpu-debug
ctest --preset linux-cpu-debug --output-on-failure
./build/linux-cpu-debug/raypalette_gui

# CUDA build (requires CUDA Toolkit and an NVIDIA GPU)
cmake --preset linux-debug
cmake --build --preset linux-debug --target format
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
./build/linux-debug/raypalette_gui
