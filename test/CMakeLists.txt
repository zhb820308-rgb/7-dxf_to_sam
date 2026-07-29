cmake_minimum_required(VERSION 4.4.0)

# ---- Google Test via FetchContent ----
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://gitee.com/mirrors/googletest.git
    GIT_TAG        v1.15.2
)
# Prevent gtest from installing itself
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---- Qt5 (for QString in DxfData) ----
find_package(Qt5Core REQUIRED)

# ---- Source files under test ----
# Compile them directly into the test executable rather than linking Example1.pyd
set(TESTABLE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/Example1/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/Example1/DxfData.cpp
    ${CMAKE_SOURCE_DIR}/src/Example1/ConversionEngine.cpp
)

# ---- Test executables ----

# Test 1: GeometryUtils (pure math)
add_executable(test_geometry_utils
    test_geometry_utils.cpp
    ${TESTABLE_SOURCES}
)
target_include_directories(test_geometry_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Example1
)
target_link_libraries(test_geometry_utils PRIVATE
    GTest::gtest_main
    Qt5::Core
)
add_test(NAME GeometryUtils COMMAND test_geometry_utils)

# Test 2: DxfData (entity validation)
add_executable(test_dxf_data
    test_dxf_data.cpp
    ${TESTABLE_SOURCES}
)
target_include_directories(test_dxf_data PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Example1
)
target_link_libraries(test_dxf_data PRIVATE
    GTest::gtest_main
    Qt5::Core
)
add_test(NAME DxfData COMMAND test_dxf_data)

# Test 3: ConversionEngine (tessellation)
add_executable(test_conversion_engine
    test_conversion_engine.cpp
    ${TESTABLE_SOURCES}
)
target_include_directories(test_conversion_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Example1
)
target_link_libraries(test_conversion_engine PRIVATE
    GTest::gtest_main
    Qt5::Core
)
add_test(NAME ConversionEngine COMMAND test_conversion_engine)

# Test 4: ConversionEngine::convert() integration
add_executable(test_convert
    test_convert.cpp
    ${TESTABLE_SOURCES}
)
target_include_directories(test_convert PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Example1
)
target_link_libraries(test_convert PRIVATE
    GTest::gtest_main
    Qt5::Core
)
add_test(NAME Convert COMMAND test_convert)

# Test 5: DxfParser integration (needs dxfrw.lib + real DXF files)
add_executable(test_parser
    test_parser.cpp
    ${TESTABLE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/Example1/DxfParser.cpp
)
target_include_directories(test_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Example1
    ${LIBDXFRW_INCLUDE_DIR}
)
target_link_libraries(test_parser PRIVATE
    GTest::gtest_main
    Qt5::Core
    "${LIBDXFRW_LIBRARY}"
)
add_test(NAME Parser COMMAND test_parser)

# MSVC: UTF-8 source encoding
if(MSVC)
    add_compile_options("/utf-8")
endif()

# ---- Runtime: ensure Qt5 DLLs are in PATH for test executables ----
set_tests_properties(GeometryUtils DxfData ConversionEngine Convert Parser PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\\;${LIBDXFRW_ROOT}/build/Release\\;$ENV{PATH}"
)
