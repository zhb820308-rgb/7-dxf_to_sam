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

# ---- Common test dependencies ----
set(TEST_COMMON_LIBS
    GTest::gtest_main
    Example1Core
)

# ---- Test executables ----

# Test 1: GeometryUtils (pure math)
add_executable(test_geometry_utils
    test_geometry_utils.cpp
)
target_link_libraries(test_geometry_utils PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME GeometryUtils COMMAND test_geometry_utils)

# Test 2: DxfData (entity validation)
add_executable(test_dxf_data
    test_dxf_data.cpp
)
target_link_libraries(test_dxf_data PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME DxfData COMMAND test_dxf_data)

# Test 3: ConversionEngine (tessellation)
add_executable(test_conversion_engine
    test_conversion_engine.cpp
)
target_link_libraries(test_conversion_engine PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME ConversionEngine COMMAND test_conversion_engine)

# Test 4: ConversionEngine::convert() integration
add_executable(test_convert
    test_convert.cpp
)
target_link_libraries(test_convert PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME Convert COMMAND test_convert)

# Test 5: FeConversionEngine (pure data conversion)
add_executable(test_fe_conversion_engine
    test_fe_conversion_engine.cpp
)
target_link_libraries(test_fe_conversion_engine PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME FeConversionEngine COMMAND test_fe_conversion_engine)

# Test 6: DxfParser integration (needs dxfrw.lib + real DXF files)
add_executable(test_parser
    test_parser.cpp
)
target_link_libraries(test_parser PRIVATE
    ${TEST_COMMON_LIBS}
    Example1DxfParser
)
target_compile_definitions(test_parser PRIVATE
    DXF_TEST_EXAMPLE_DIR="${CMAKE_SOURCE_DIR}/example"
    DXF_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)
add_test(NAME Parser COMMAND test_parser)

# ---- Runtime: ensure Qt5 DLLs are in PATH for test executables ----
set_tests_properties(
    GeometryUtils DxfData ConversionEngine Convert FeConversionEngine Parser
    PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\\;${LIBDXFRW_ROOT}/build/Release\\;${SAM_OCCT_BIN_DIR}\\;$ENV{PATH}"
)

# Test 7: DxfImportLogger (needs QCoreApplication + spdlog, custom main)
add_executable(test_dxf_import_logger
    test_dxf_import_logger.cpp
)
target_link_libraries(test_dxf_import_logger PRIVATE
    GTest::gtest
    Example1Core
)
add_test(NAME DxfImportLogger COMMAND test_dxf_import_logger)

set_tests_properties(DxfImportLogger PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\\;${SPDLOG_ROOT}/bin\\;${SAM_OCCT_BIN_DIR}\\;$ENV{PATH}"
)

# Test 8: shared builder transaction state machine (pure C++)
add_executable(test_import_transaction test_import_transaction.cpp)
target_include_directories(test_import_transaction PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Example1
)
target_link_libraries(test_import_transaction PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME ImportTransaction COMMAND test_import_transaction)
set_tests_properties(ImportTransaction PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\;$ENV{PATH}"
)

# Test 9: import numeric parameters and output budget validation
add_executable(test_dxf_import_validation
    test_dxf_import_validation.cpp
)
target_link_libraries(test_dxf_import_validation PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME DxfImportValidation COMMAND test_dxf_import_validation)
set_tests_properties(DxfImportValidation PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\;$ENV{PATH}"
)

# Test 10: Sketch/FE mode selection and required FE names
add_executable(test_dxf_import_mode
    test_dxf_import_mode.cpp
)
target_link_libraries(test_dxf_import_mode PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME DxfImportMode COMMAND test_dxf_import_mode)
set_tests_properties(DxfImportMode PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\;$ENV{PATH}"
)

# Test 11: ignored-layer option parsing
add_executable(test_dxf_import_layers
    test_dxf_import_layers.cpp
)
target_link_libraries(test_dxf_import_layers PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME DxfImportLayers COMMAND test_dxf_import_layers)
set_tests_properties(DxfImportLayers PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\;$ENV{PATH}"
)

# Test 12: BLOCK/INSERT recursion, depth and cumulative budget boundaries
add_executable(test_dxf_block_expansion
    test_dxf_block_expansion.cpp
)
target_link_libraries(test_dxf_block_expansion PRIVATE
    ${TEST_COMMON_LIBS}
    Example1DxfParser
)
target_compile_definitions(test_dxf_block_expansion PRIVATE
    DXF_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)
add_test(NAME DxfBlockExpansion COMMAND test_dxf_block_expansion)
set_tests_properties(DxfBlockExpansion PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\;${LIBDXFRW_ROOT}/build/Release\;${SAM_OCCT_BIN_DIR}\;$ENV{PATH}"
)

# Test 13: transformed BLOCK curves and Sketch/FE geometry parity
add_executable(test_dxf_transformed_curves
    test_dxf_transformed_curves.cpp
)
target_link_libraries(test_dxf_transformed_curves PRIVATE
    ${TEST_COMMON_LIBS}
    Example1DxfParser
)
add_test(NAME DxfTransformedCurves COMMAND test_dxf_transformed_curves)
set_tests_properties(DxfTransformedCurves PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\;${LIBDXFRW_ROOT}/build/Release\;${SAM_OCCT_BIN_DIR}\;$ENV{PATH}"
)

# Test 14: builder orchestration failure, cancellation and compensation
add_executable(test_import_build_service
    test_import_build_service.cpp
)
target_link_libraries(test_import_build_service PRIVATE ${TEST_COMMON_LIBS})
add_test(NAME ImportBuildService COMMAND test_import_build_service)
set_tests_properties(ImportBuildService PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt5::Core>\;${SAM_OCCT_BIN_DIR}\;$ENV{PATH}"
)
