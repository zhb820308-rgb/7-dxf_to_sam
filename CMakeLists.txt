cmake_minimum_required(VERSION 4.4.0)

# 初学者导航：CMakeLists.txt 不是 C++ 源码，而是“怎样把源码和 SDK 拼成产物”的构建
# 配方。推荐先读到文件末尾的 add_subdirectory：根文件负责依赖/公共选项/输出目录，
# src/Example1 和 src/Example1Toolset 再分别声明 .pyd 与 .dll 的源码和链接库。
# 本项目不会生成独立 .exe：宿主进程是 SAM.exe，本仓库生成被它加载的两个动态模块。

# disable in-source build
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

project(Example)

# 3rdParty/ 用来放实际第三方库内容，不放额外的 CMake 检索文档。
# 以后把依赖复制进该目录后，下面各依赖会优先采用仓库内版本；如果目录不存在，
# 仍回退到原来的开发环境路径。-D 参数和环境变量始终拥有更高优先级。
set(EXAMPLE1_3RDPARTY_ROOT "${CMAKE_SOURCE_DIR}/3rdParty" CACHE PATH
    "Root directory containing project-local third-party libraries")
list(PREPEND CMAKE_PREFIX_PATH "${EXAMPLE1_3RDPARTY_ROOT}")

# -- LIBS_SAM_ROOT --
if(NOT LIBS_SAM_ROOT)
    if(DEFINED ENV{SAM_ROOT})
        set(LIBS_SAM_ROOT "$ENV{SAM_ROOT}" CACHE PATH
            "SAM installation root directory")
    elseif(EXISTS "${EXAMPLE1_3RDPARTY_ROOT}/SAM")
        set(LIBS_SAM_ROOT "${EXAMPLE1_3RDPARTY_ROOT}/SAM" CACHE PATH
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
    elseif(EXISTS "${EXAMPLE1_3RDPARTY_ROOT}/SAMSDK")
        set(LIBS_SAMSDK_ROOT "${EXAMPLE1_3RDPARTY_ROOT}/SAMSDK" CACHE PATH
            "SAMSDK root directory")
    else()
        set(LIBS_SAMSDK_ROOT
            "${CMAKE_SOURCE_DIR}/../../OpenOLTranSim-main/SAMSDK"
            CACHE PATH "SAMSDK root directory")
    endif()
endif()

# -- LIBDXFRW_ROOT --
if(NOT LIBDXFRW_ROOT)
    if(DEFINED ENV{LIBDXFRW_ROOT})
        set(LIBDXFRW_ROOT "$ENV{LIBDXFRW_ROOT}" CACHE PATH
            "libdxfrw root directory")
    elseif(EXISTS "${EXAMPLE1_3RDPARTY_ROOT}/libdxfrw")
        set(LIBDXFRW_ROOT "${EXAMPLE1_3RDPARTY_ROOT}/libdxfrw" CACHE PATH
            "libdxfrw root directory")
    else()
        set(LIBDXFRW_ROOT "${CMAKE_SOURCE_DIR}/../env/libdxfrw" CACHE PATH
            "libdxfrw root directory")
    endif()
endif()

# -- SPDLOG_ROOT --
if(NOT SPDLOG_ROOT)
    # CACHE 里可能残留上一次配置写入的空值。普通 set(... CACHE ...) 不会覆盖已有
    # cache entry，因此这里在确认当前值为空后使用 FORCE，把自动发现结果写回缓存。
    if(DEFINED ENV{SPDLOG_ROOT})
        set(SPDLOG_ROOT "$ENV{SPDLOG_ROOT}" CACHE PATH
            "Root directory containing the installed spdlog package" FORCE)
    elseif(EXISTS "${EXAMPLE1_3RDPARTY_ROOT}/spdlog")
        set(SPDLOG_ROOT "${EXAMPLE1_3RDPARTY_ROOT}/spdlog" CACHE PATH
            "Root directory containing the installed spdlog package" FORCE)
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(SPDLOG_ROOT "$ENV{VCPKG_ROOT}/installed/x64-windows" CACHE PATH
            "Root directory containing the installed spdlog package" FORCE)
    else()
        set(SPDLOG_ROOT "" CACHE PATH
            "Root directory containing the installed spdlog package" FORCE)
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
set(LIBDXFRW_LIBRARY "${LIBDXFRW_ROOT}/lib/dxfrw.lib")
set(LIBDXFRW_RUNTIME "${LIBDXFRW_ROOT}/bin/dxfrw.dll")
set(spdlog_DIR "${SPDLOG_ROOT}/share/spdlog")
if(NOT EXISTS "${spdlog_DIR}/spdlogConfig.cmake" AND
   EXISTS "${SPDLOG_ROOT}/lib/cmake/spdlog/spdlogConfig.cmake")
    set(spdlog_DIR "${SPDLOG_ROOT}/lib/cmake/spdlog")
endif()

# OCCT 已迁入项目统一的实际三方库目录；仍可用 -DSAM_OCCT_ROOT 覆盖。
set(SAM_OCCT_ROOT "${EXAMPLE1_3RDPARTY_ROOT}/OCCT" CACHE PATH
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
add_library(libdxfrw::dxfrw SHARED IMPORTED GLOBAL)
set_target_properties(libdxfrw::dxfrw PROPERTIES
    IMPORTED_IMPLIB "${LIBDXFRW_LIBRARY}"
    IMPORTED_LOCATION "${LIBDXFRW_RUNTIME}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBDXFRW_INCLUDE_DIR}"
)
if(NOT EXISTS "${spdlog_DIR}/spdlogConfig.cmake")
    message(FATAL_ERROR
        "spdlogConfig.cmake not found: ${spdlog_DIR}. "
        "Set SPDLOG_ROOT or install spdlog under 3rdParty/spdlog.")
endif()

add_library(ExampleProjectOptions INTERFACE)
# 公共头文件使用 std::optional 等 C++17 类型。用 INTERFACE 编译特性把要求
# 传播给插件、核心库和单元测试；只给库设置 CXX_STANDARD 不会影响包含其头文件的调用方。
target_compile_features(ExampleProjectOptions INTERFACE cxx_std_17)
target_compile_definitions(ExampleProjectOptions INTERFACE
    NEW_GUI_FRAME
    NEW_RENDER_FRAME
)

# MSVC: treat project source files as UTF-8 (no BOM required).
if(MSVC)
    target_compile_options(ExampleProjectOptions INTERFACE "/utf-8")
endif()

list(APPEND CMAKE_PREFIX_PATH
    ${LIBS_QT5_ROOT}/lib/cmake/Qt5
    ${LIBS_QT5_ROOT}/lib/cmake/Qt5Widgets
    ${LIBS_QT5_ROOT}/lib/cmake/Qt5Core
    ${LIBS_QT5_ROOT}/lib/cmake/Qt5Gui
)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# output paths
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
# Windows 的 DLL/PYD 属于 runtime 产物，因此最终会落在 bin/<配置>/ 下；
# 静态库/导入库通常落在 lib/<配置>/。Visual Studio 是多配置生成器，<配置> 常为 Release。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

# add_subdirectory 类似“继续执行子目录的 CMakeLists.txt”，不是把目录源码自动全加进来。
add_subdirectory(src/Example1)
add_subdirectory(src/Example1Toolset)

option(EXAMPLE1_BUILD_TESTS "Build Example1 unit tests" OFF)
if(EXAMPLE1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
