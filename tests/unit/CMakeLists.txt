add_executable(raypalette_unit_tests
    color_test.cpp
    geometry_test.cpp
    polar_test.cpp
    vec3_test.cpp
)

target_link_libraries(raypalette_unit_tests PRIVATE GTest::gtest_main)
target_include_directories(raypalette_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(raypalette_unit_tests)