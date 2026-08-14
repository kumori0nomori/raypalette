add_executable(raypalette_cpu_tests
    renderer_test.cpp
)

target_link_libraries(raypalette_cpu_tests
    PRIVATE
        raypalette_renderer
        GTest::gtest_main
)
target_include_directories(raypalette_cpu_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(raypalette_cpu_tests)