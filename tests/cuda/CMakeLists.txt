add_executable(raypalette_cuda_tests
    renderer_test.cu
)

target_link_libraries(raypalette_cuda_tests
    PRIVATE
        raypalette_renderer
        GTest::gtest_main
)
target_include_directories(raypalette_cuda_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(raypalette_cuda_tests)