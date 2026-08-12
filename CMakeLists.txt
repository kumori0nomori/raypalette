cmake_minimum_required(VERSION 3.24)

project(RayPalette
    VERSION 0.1.0
    DESCRIPTION "A lightweight CUDA renderer for illustration color studies"
    LANGUAGES CXX CUDA
)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_EXTENSIONS OFF)

if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 12.8)
    message(FATAL_ERROR
        "RayPalette requires CUDA Toolkit 12.8 or newer for RTX 50 series (sm_120). "
        "Detected CUDA ${CMAKE_CUDA_COMPILER_VERSION}.")
endif()

set(CMAKE_CUDA_ARCHITECTURES 120 CACHE STRING
    "RayPalette supports RTX 50 series GPUs (sm_120) only." FORCE)

option(RAYPALETTE_BUILD_GUI "Build the native GUI application" OFF)
option(RAYPALETTE_FETCH_DEPENDENCIES "Fetch optional third-party dependencies" OFF)

add_executable(raypalette
    src/app/main.cpp
)

add_library(raypalette_renderer STATIC
    src/render/renderer.cu
)
target_include_directories(raypalette_renderer PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_compile_features(raypalette PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(raypalette PRIVATE /W4 /permissive-)
else()
    target_compile_options(raypalette PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    include(cmake/Dependencies.cmake)
    raypalette_fetch_test_dependencies()
    add_subdirectory(tests)
endif()

add_library(raypalette_cuda_compile_check OBJECT
    tests/cuda/math_compile_check.cu
)
target_include_directories(raypalette_cuda_compile_check PRIVATE ${PROJECT_SOURCE_DIR}/src)

if(RAYPALETTE_FETCH_DEPENDENCIES)
    include(cmake/Dependencies.cmake)
    raypalette_fetch_gui_dependencies()
endif()

if(RAYPALETTE_BUILD_GUI)
    message(FATAL_ERROR
        "The GUI target has not been implemented yet. Configure with "
        "-DRAYPALETTE_BUILD_GUI=OFF.")
endif()

message(STATUS "RayPalette CUDA target architecture: sm_${CMAKE_CUDA_ARCHITECTURES}")