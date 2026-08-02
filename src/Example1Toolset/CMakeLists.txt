set(EXAMPLE1_TOOLSET_SOURCES
    DxfLayerReader.cpp
    DxfLayerReader.h
    DxfLogViewerDialog.cpp
    DxfLogViewerDialog.h
    Example1DB.cpp
    Example1DB.h
    Example1DXFImportDialog.cpp
    Example1DXFImportDialog.h
    Example1Form.cpp
    Example1Form.h
    Example1ToolsetGui.cpp
    Example1ToolsetGui.h
    Example1ToolsetPlugin.cpp
    Example1ToolsetPlugin.h
    MultiSelectComboBox.cpp
    MultiSelectComboBox.h
)

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

