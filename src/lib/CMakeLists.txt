set (HSPP_SOURCES
    hspp/database.cpp
)

add_library(libhspp STATIC ${HSPP_SOURCES})
target_include_directories(libhspp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(libhspp PROPERTIES
    PREFIX ""
    OUTPUT_NAME "hspp"
)
target_link_libraries(libhspp HyperScan::HyperScan)

add_library(libhspp_runtime STATIC ${HSPP_SOURCES})
target_compile_definitions(libhspp_runtime PUBLIC ONLY_RUNTIME)
target_include_directories(libhspp_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(libhspp_runtime PROPERTIES
    PREFIX ""
    OUTPUT_NAME "hspp_runtime"
)
target_link_libraries(libhspp_runtime HyperScan::HyperScanRuntime)

set(YARANG_SOURCES
    yarang/ruleset.cpp
)

add_library(libyarang STATIC ${YARANG_SOURCES})
target_include_directories(libyarang PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(libyarang PROPERTIES
    PREFIX ""
    OUTPUT_NAME "yarang"
)
target_link_libraries(libyarang Threads::Threads ${CMAKE_DL_LIBS})

set(YARANGC_SOURCES
    yarangc/conversion.cpp
)

add_library(libyarangc STATIC ${YARANGC_SOURCES})
target_include_directories(libyarangc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(libyarangc PROPERTIES
    PREFIX ""
    OUTPUT_NAME "yarangc"
)
target_link_libraries(libyarangc Yaramod::Yaramod Threads::Threads)
