rm -rf build

cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
./build/linux-debug/raypalette

cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release --output-on-failure
./build/linux-release/raypalette

cmake --preset linux-gui-debug
cmake --build --preset linux-gui-debug
ctest --preset linux-gui-debug --output-on-failure
./build/linux-gui-debug/raypalette_gui

cmake --preset linux-gui-release
cmake --build --preset linux-gui-release
ctest --preset linux-gui-release --output-on-failure
./build/linux-gui-release/raypalette_gui