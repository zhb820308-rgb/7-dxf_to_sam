cmake_minimum_required(VERSION 3.31.11)

# disable in-source build
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

project(Example)

get_filename_component(current_folder_absolute ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(current_folder_absolute ${current_folder_absolute} DIRECTORY)
get_filename_component(current_folder_absolute ${current_folder_absolute} DIRECTORY)
get_filename_component(current_dir ${current_folder_absolute} DIRECTORY)

# set libs
set(LIBS_SAM_ROOT "D:/shixi_application/SAM")
set(LIBS_SAMSDK_ROOT "D:/shixi_application/zipfile/SAMSDK")
set(SPDLOG_ROOT "D:/code/CADhomework/spdlog" CACHE PATH
    "Root directory containing the installed shared spdlog package")
set(LIBS_PYTHON_ROOT ${LIBS_SAM_ROOT}/Python27)
set(LIBS_QT5_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/Qt-5.12.6-VC14-64)
set(LIBS_OCCT_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/OCCT)
set(LIBS_TCMALLOC_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/TCMalloc)
set(LIBS_LEVELDB_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/libdb)
set(LIBS_HDF5_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/HDF5-1.12)
set(LIBS_CM2MeshTool_Root ${LIBS_SAMSDK_ROOT}/ThridPartys/cm2)
set(LIBS_OC_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/OrientCommon)
set(LIBS_QTITAN_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/Qtitan)
set(LIBDXFRW_ROOT "D:/code/CADhomework/libdxfrw/libdxfrw")
set(LIBDXFRW_INCLUDE_DIR "${LIBDXFRW_ROOT}/src")
set(LIBDXFRW_LIBRARY "${LIBDXFRW_ROOT}/build/Release/dxfrw.lib")
set(LIBDXFRW_RUNTIME "${LIBDXFRW_ROOT}/build/Release/dxfrw.dll")
set(spdlog_DIR "${SPDLOG_ROOT}/install-shared/lib/cmake/spdlog")

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

add_compile_definitions(NEW_GUI_FRAME)
add_compile_definitions(NEW_RENDER_FRAME)

# MSVC: treat source files as UTF-8 (no BOM required)
if(MSVC)
    add_compile_options("/utf-8")
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

