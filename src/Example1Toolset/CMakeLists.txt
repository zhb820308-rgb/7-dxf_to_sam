# 这个目标只放 Qt/SAM GUI 文件。真正解析与转换代码位于相邻 Example1 目标，
# GUI 通过 SAM 命令系统调用 Example1.pyd，而不是把核心 .cpp 再编译一份。
set(EXAMPLE1_TOOLSET_SOURCES
    DxfImportDialog.cpp
    DxfImportDialog.h
    DxfLogViewerDialog.cpp
    DxfLogViewerDialog.h
    Example1Form.cpp
    Example1Form.h
    Example1Toolset.cpp
    Example1Toolset.h
    MultiSelectComboBox.cpp
    MultiSelectComboBox.h
)

# 生成 SAM 扫描并加载的 Qt 插件 DLL。
add_library(Example1Toolset SHARED ${EXAMPLE1_TOOLSET_SOURCES})
target_include_directories(Example1Toolset PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBS_QTITAN_ROOT}/include
    ${LIBS_PYTHON_ROOT}/include
    ${LIBS_OC_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include/guiProject
)
target_link_directories(Example1Toolset PRIVATE
    ${LIBS_QTITAN_ROOT}/lib
    ${LIBS_PYTHON_ROOT}/libs
    ${LIBS_OC_ROOT}/lib
    ${LIBS_SAMSDK_ROOT}/libs
    ${LIBS_TCMALLOC_ROOT}
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
)
target_compile_definitions(Example1Toolset PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NON_CONFORMING_SWPRINTFS
    UNICODE
    _UNICODE
    QT_NO_DEBUG
)

set_target_properties(Example1Toolset PROPERTIES
    # SAM 约定的部署名是 SAM.Pre.Example1Toolset.dll；OUTPUT_NAME 改主文件名，
    # SUFFIX 明确 Windows 动态库后缀。它不是可双击运行的 exe。
    AUTOMOC ON
    OUTPUT_NAME "SAM.Pre.Example1Toolset"
    SUFFIX ".dll"
)

if(MSVC)
    target_link_options(Example1Toolset PRIVATE "/INCLUDE:__tcmalloc")
endif()

target_link_libraries(Example1Toolset
    ExampleProjectOptions
    Example1DxfInput
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    OrientBasicPrimtives
    SAMIngQtSAM
    SAMGui
    libtcmalloc_minimal
    libdxfrw::dxfrw
)
