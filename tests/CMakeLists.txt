find_package(GTest REQUIRED)
include(GoogleTest)

set(SOURCES
    test_codegen.cpp
    test_conversion.cpp
)

add_executable(yarang_tests ${SOURCES})
target_link_libraries(yarang_tests libyarang libyarangc GTest::GTest GTest::Main)
gtest_discover_tests(yarang_tests)
