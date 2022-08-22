add_subdirectory(lib)

set(SCANNER_SOURCES
    yarang.cpp
)

set(COMPILER_SOURCES
    yarangc.cpp
)

add_executable(yarang ${SCANNER_SOURCES})
target_link_libraries(yarang PUBLIC libyarang HyperScan::HyperScanRuntime)

add_executable(yarangc ${COMPILER_SOURCES})
target_link_libraries(yarangc PUBLIC libyarangc HyperScan::HyperScan)
