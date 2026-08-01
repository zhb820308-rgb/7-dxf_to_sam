cmake_minimum_required(VERSION 4.4.0)

# disable in-source build
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

project(Example)

# ---- Dependency path resolution ----
# Priority: cmake -D<VAR>=... > environment variable > project-relative default
#
# Each <VAR> can be overridden at configure time:
#   cmake -DLIBS_SAM_ROOT="C:/path/to/SAM" ..
# or by setting the corresponding environment variable before configuring.
#
# Project-relative defaults assume the development layout:
#   CMAKE_SOURCE_DIR/../../SAM
#   CMAKE_SOURCE_DIR/../../OpenOLTranSim-main/SAMSDK
#   CMAKE_SOURCE_DIR/../env/libdxfrw
#   (spdlog has no sensible relative path — falls back to env or old default)

# -- LIBS_SAM_ROOT --
if(NOT LIBS_SAM_ROOT)
    if(DEFINED ENV{SAM_ROOT})
        set(LIBS_SAM_ROOT "$ENV{SAM_ROOT}" CACHE PATH
            "SAM installation root directory")
    else()
        set(LIBS_SAM_ROOT "${CMAKE_SOURCE_DIR}/../../SAM" CACHE PATH
            "SAM installation root directory")
    endif()
endif()

# -- LIBS_SAMSDK_ROOT --
if(NOT LIBS_SAMSDK_ROOT)
    if(DEFINED ENV{SAMSDK_ROOT})
        set(LIBS_SAMSDK_ROOT "$ENV{SAMSDK_ROOT}" CACHE PATH
            "SAMSDK root directory")
    else()
        set(LIBS_SAMSDK_ROOT "${CMAKE_SOURCE_DIR}/../../OpenOLTranSim-main/SAMSDK" CACHE PATH
            "SAMSDK root directory")
    endif()
endif()

# -- LIBDXFRW_ROOT --
if(NOT LIBDXFRW_ROOT)
    if(DEFINED ENV{LIBDXFRW_ROOT})
        set(LIBDXFRW_ROOT "$ENV{LIBDXFRW_ROOT}" CACHE PATH
            "libdxfrw root directory")
    else()
        set(LIBDXFRW_ROOT "${CMAKE_SOURCE_DIR}/../env/libdxfrw" CACHE PATH
            "libdxfrw root directory")
    endif()
endif()

# -- SPDLOG_ROOT --
if(NOT SPDLOG_ROOT)
    if(DEFINED ENV{SPDLOG_ROOT})
        set(SPDLOG_ROOT "$ENV{SPDLOG_ROOT}" CACHE PATH
            "Root directory containing the installed spdlog package")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(SPDLOG_ROOT "$ENV{VCPKG_ROOT}/installed/x64-windows" CACHE PATH
            "Root directory containing the installed spdlog package")
    else()
        set(SPDLOG_ROOT "D:/vcpkg/installed/x64-windows" CACHE PATH
            "Root directory containing the installed spdlog package")
    endif()
endif()

# FE libraries are included in the local SAMSDK distribution.
if(NOT LIBS_FEASOFT_ROOT)
    if(DEFINED ENV{FEASOFT_ROOT})
        set(LIBS_FEASOFT_ROOT "$ENV{FEASOFT_ROOT}" CACHE PATH
            "FEASoft installation root directory")
    else()
        set(LIBS_FEASOFT_ROOT "${LIBS_SAMSDK_ROOT}" CACHE PATH
            "FEASoft-compatible SAMSDK root directory")
    endif()
endif()

# Derived paths
set(LIBS_PYTHON_ROOT ${LIBS_SAM_ROOT}/Python27)
set(LIBS_QT5_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/Qt-5.12.6-VC14-64)
set(LIBS_TCMALLOC_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/TCMalloc)
set(LIBS_LEVELDB_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/libdb)
set(LIBS_HDF5_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/HDF5-1.12)
set(LIBS_CM2MeshTool_Root ${LIBS_SAMSDK_ROOT}/ThridPartys/cm2)
set(LIBS_OC_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/OrientCommon)
set(LIBS_QTITAN_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/Qtitan)
set(LIBDXFRW_INCLUDE_DIR "${LIBDXFRW_ROOT}/src")
set(LIBDXFRW_LIBRARY "${LIBDXFRW_ROOT}/build/Release/dxfrw.lib")
set(LIBDXFRW_RUNTIME "${LIBDXFRW_ROOT}/build/Release/dxfrw.dll")
set(spdlog_DIR "${SPDLOG_ROOT}/share/spdlog")
if(NOT EXISTS "${spdlog_DIR}/spdlogConfig.cmake" AND
   EXISTS "${SPDLOG_ROOT}/lib/cmake/spdlog/spdlogConfig.cmake")
    # Standard CMake install layout used by upstream spdlog builds.
    set(spdlog_DIR "${SPDLOG_ROOT}/lib/cmake/spdlog")
endif()

# OCCT (Open CASCADE Technology) — headers in repo, libs configurable
set(SAM_OCCT_ROOT "${CMAKE_SOURCE_DIR}/example/OCCT" CACHE PATH
    "OCCT root directory (contains inc/ and win64/vc14/lib/)")
set(SAM_OCCT_INCLUDE_DIR "${SAM_OCCT_ROOT}/inc")
set(SAM_OCCT_LIB_DIR "${SAM_OCCT_ROOT}/win64/vc14/lib")
set(SAM_OCCT_BIN_DIR "${SAM_OCCT_ROOT}/win64/vc14/bin")

# check libdxfrw files
if(NOT EXISTS "${LIBDXFRW_INCLUDE_DIR}/libdxfrw.h")
    message(FATAL_ERROR "libdxfrw.h not found: ${LIBDXFRW_INCLUDE_DIR}/libdxfrw.h")
endif()
if(NOT EXISTS "${LIBDXFRW_LIBRARY}")
    message(FATAL_ERROR "dxfrw.lib not found: ${LIBDXFRW_LIBRARY}")
endif()
if(NOT EXISTS "${LIBDXFRW_RUNTIME}")
    message(FATAL_ERROR "dxfrw.dll not found: ${LIBDXFRW_RUNTIME}")
endif()
if(NOT EXISTS "${spdlog_DIR}/spdlogConfig.cmake")
    message(FATAL_ERROR
        "spdlogConfig.cmake not found: ${spdlog_DIR}. "
        "Set SPDLOG_ROOT to the spdlog root directory.")
endif()

add_library(ExampleProjectOptions INTERFACE)
target_compile_definitions(ExampleProjectOptions INTERFACE
    NEW_GUI_FRAME
    NEW_RENDER_FRAME
)

# MSVC: treat project source files as UTF-8 (no BOM required).
if(MSVC)
    target_compile_options(ExampleProjectOptions INTERFACE "/utf-8")
endif()

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5Widgets)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5Core)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5Gui)

# check Qt library
find_package(Qt5Widgets REQUIRED) 
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# output paths
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

add_subdirectory(src/Example1)
add_subdirectory(src/Example1Toolset)

option(EXAMPLE1_BUILD_CONTAINER_SHIP_SECTION
    "Build the optional ContainerShipSection modules" OFF)
if(EXAMPLE1_BUILD_CONTAINER_SHIP_SECTION)
    add_subdirectory(ContainerShipSection)
    add_subdirectory(ContainerShipSectionToolset/ContainerShipSectionToolset)
endif()

option(EXAMPLE1_BUILD_TESTS "Build Example1 unit tests" OFF)
if(EXAMPLE1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
