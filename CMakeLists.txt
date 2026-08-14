cmake_minimum_required(VERSION 3.24)

project(RayPalette
    VERSION 0.1.0
    DESCRIPTION "A lightweight CUDA renderer for illustration color studies"
    LANGUAGES CXX
)

include(CTest)
include(CheckLanguage)

option(RAYPALETTE_ENABLE_CUDA "Build the CUDA renderer" ON)

if(RAYPALETTE_ENABLE_CUDA)
    check_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER)
        message(FATAL_ERROR
            "RAYPALETTE_ENABLE_CUDA=ON requires a CUDA Toolkit. "
            "Configure with -DRAYPALETTE_ENABLE_CUDA=OFF for the CPU renderer.")
    endif()
    enable_language(CUDA)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(RAYPALETTE_ENABLE_CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    set(CMAKE_CUDA_EXTENSIONS OFF)

    if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 12.8)
        message(FATAL_ERROR
            "RayPalette requires CUDA Toolkit 12.8 or newer for RTX 50 series (sm_120). "
            "Detected CUDA ${CMAKE_CUDA_COMPILER_VERSION}.")
    endif()

    set(CMAKE_CUDA_ARCHITECTURES 120 CACHE STRING
        "RayPalette supports RTX 50 series GPUs (sm_120) only.")
endif()

option(RAYPALETTE_BUILD_GUI "Build the native GUI application" OFF)
option(RAYPALETTE_FETCH_DEPENDENCIES "Fetch optional third-party dependencies" OFF)

set(RAYPALETTE_FORMAT_FILES
    ${PROJECT_SOURCE_DIR}/src/app/application.cpp
    ${PROJECT_SOURCE_DIR}/src/math/color.hpp
    ${PROJECT_SOURCE_DIR}/src/math/polar.hpp
    ${PROJECT_SOURCE_DIR}/src/math/vec3.hpp
    ${PROJECT_SOURCE_DIR}/src/render/camera.hpp
    ${PROJECT_SOURCE_DIR}/src/render/bsdf.hpp
    ${PROJECT_SOURCE_DIR}/src/render/geometry.hpp
    ${PROJECT_SOURCE_DIR}/src/render/image.hpp
    ${PROJECT_SOURCE_DIR}/src/render/light.hpp
    ${PROJECT_SOURCE_DIR}/src/render/light_sampling.hpp
    ${PROJECT_SOURCE_DIR}/src/render/material.hpp
    ${PROJECT_SOURCE_DIR}/src/render/ray.hpp
    ${PROJECT_SOURCE_DIR}/src/render/renderer_cpu.cpp
    ${PROJECT_SOURCE_DIR}/src/render/renderer_cuda.cu
    ${PROJECT_SOURCE_DIR}/src/render/renderer.hpp
    ${PROJECT_SOURCE_DIR}/src/render/tracer.hpp
    ${PROJECT_SOURCE_DIR}/src/render/scene.hpp
    ${PROJECT_SOURCE_DIR}/src/ui/hsv_plot.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/hsv_plot.hpp
    ${PROJECT_SOURCE_DIR}/src/ui/hsv_plot_gui.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/hsv_plot_gui.hpp
    ${PROJECT_SOURCE_DIR}/src/ui/controls.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/controls.hpp
    ${PROJECT_SOURCE_DIR}/src/ui/gui_state.hpp
    ${PROJECT_SOURCE_DIR}/src/ui/palette.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/palette.hpp
    ${PROJECT_SOURCE_DIR}/src/ui/preview.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/preview.hpp
    ${PROJECT_SOURCE_DIR}/src/ui/texture.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/texture.hpp
    ${PROJECT_SOURCE_DIR}/tests/cpu/renderer_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/color_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/geometry_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/hsv_plot_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/palette_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/polar_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/scene_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/vec3_test.cpp
)

find_program(RAYPALETTE_CLANG_FORMAT clang-format)
if(RAYPALETTE_CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${RAYPALETTE_CLANG_FORMAT} -i ${RAYPALETTE_FORMAT_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Formatting C++ and CUDA sources with clang-format"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${RAYPALETTE_CLANG_FORMAT} --dry-run --Werror ${RAYPALETTE_FORMAT_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Checking C++ and CUDA formatting with clang-format"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo
            "clang-format was not found; install it to use the format target."
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E echo
            "clang-format was not found; install it to use the format-check target."
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()

if(RAYPALETTE_ENABLE_CUDA)
    set(RAYPALETTE_RENDERER_SOURCE src/render/renderer_cuda.cu)
else()
    set(RAYPALETTE_RENDERER_SOURCE src/render/renderer_cpu.cpp)
endif()

add_library(raypalette_renderer STATIC ${RAYPALETTE_RENDERER_SOURCE})
target_include_directories(raypalette_renderer PUBLIC ${PROJECT_SOURCE_DIR}/src)

if(RAYPALETTE_ENABLE_CUDA)
    target_compile_definitions(raypalette_renderer PRIVATE RAYPALETTE_CUDA_BACKEND=1)
endif()

if(BUILD_TESTING)
    include(cmake/Dependencies.cmake)
    raypalette_fetch_test_dependencies()
    add_subdirectory(tests)
endif()

if(RAYPALETTE_ENABLE_CUDA)
    add_library(raypalette_cuda_compile_check OBJECT
        tests/cuda/math_compile_check.cu
    )
    target_include_directories(raypalette_cuda_compile_check PRIVATE ${PROJECT_SOURCE_DIR}/src)
endif()

if(RAYPALETTE_FETCH_DEPENDENCIES)
    include(cmake/Dependencies.cmake)
    if(RAYPALETTE_BUILD_GUI)
        raypalette_fetch_gui_dependencies()
    endif()
endif()

if(RAYPALETTE_BUILD_GUI)
    if(NOT RAYPALETTE_FETCH_DEPENDENCIES)
        message(FATAL_ERROR "RAYPALETTE_BUILD_GUI requires RAYPALETTE_FETCH_DEPENDENCIES=ON.")
    endif()

    add_library(raypalette_imgui STATIC
        ${RAYPALETTE_IMGUI_SOURCE_DIR}/imgui.cpp
        ${RAYPALETTE_IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${RAYPALETTE_IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${RAYPALETTE_IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${RAYPALETTE_IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${RAYPALETTE_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(raypalette_imgui PUBLIC
        ${RAYPALETTE_IMGUI_SOURCE_DIR}
        ${RAYPALETTE_IMGUI_SOURCE_DIR}/backends
    )
    target_link_libraries(raypalette_imgui PUBLIC glfw)

    find_package(OpenGL REQUIRED)
    add_executable(raypalette_gui
        src/app/application.cpp
        src/ui/hsv_plot.cpp
        src/ui/hsv_plot_gui.cpp
        src/ui/controls.cpp
        src/ui/palette.cpp
        src/ui/preview.cpp
        src/ui/texture.cpp
    )
    target_include_directories(raypalette_gui PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(raypalette_gui PRIVATE
        raypalette_renderer
        raypalette_imgui
        OpenGL::GL
    )
endif()

if(RAYPALETTE_ENABLE_CUDA)
    message(STATUS "RayPalette CUDA target architecture: sm_${CMAKE_CUDA_ARCHITECTURES}")
else()
    message(STATUS "RayPalette renderer backend: CPU")
endif()