# 自动为QObject派生类生成moc文件
set(CMAKE_AUTOMOC ON)
# 将当前目录加入头文件搜索路径
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/dialog
    ${LIBS_QT5_ROOT}/include
    ${LIBS_QT5_ROOT}/include/QtCore
    ${LIBS_QT5_ROOT}/include/QtGui
    ${LIBS_QT5_ROOT}/include/QtWidgets
    ${LIBS_QTITAN_ROOT}/include
    ${LIBS_PYTHON_ROOT}/include
    ${LIBS_OC_ROOT}/include
    ${LIBS_FEASOFT_ROOT}/include
    ${LIBS_FEASOFT_ROOT}/include/guiProject
)

# 链接目录
link_directories(
    ${LIBS_QT5_ROOT}/lib
    ${LIBS_QTITAN_ROOT}/lib
    ${LIBS_PYTHON_ROOT}/libs
    ${LIBS_OC_ROOT}/lib
    ${LIBS_FEASOFT_ROOT}/libs
    ${LIBS_TCMALLOC_ROOT}
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
)

# 源文件收集
file(GLOB TARGET_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/dialog/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dialog/*.h"
    )

# Qt处理
set(HEADERS
)
set(RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/resource.qrc"
)
QT5_WRAP_CPP(HEADERS_MOC ${HEADERS})
QT5_ADD_RESOURCES(RESOURCES_RCC ${RESOURCES})
source_group("Generated Files" FILES
    ${HEADERS_MOC}
)

# 创建动态库
add_library(ContainerShipSectionToolset SHARED
    ${TARGET_SRC}
    ${HEADERS_MOC}
    ${RESOURCES_RCC}
)

# 编译器设置
set_property(TARGET ContainerShipSectionToolset
    PROPERTY COMPILE_DEFINITIONS
    _CRT_SECURE_NO_WARNINGS
    _CRT_NON_CONFORMING_SWPRINTFS
)

# 链接器标志（tcmalloc）
set_target_properties(ContainerShipSectionToolset PROPERTIES
    LINK_FLAGS /INCLUDE:"__tcmalloc"
)

# 宏定义
add_definitions(
    -DUNICODE
    -D_UNICODE
    -DQT_NO_DEBUG
)

# 输出名称设置
set_target_properties(ContainerShipSectionToolset PROPERTIES
    OUTPUT_NAME "SAM.Pre.ContainerShipSectionToolset"
    SUFFIX ".dll"
)

# 链接库
target_link_libraries(ContainerShipSectionToolset
    Qt5Core.lib
    Qt5Widgets.lib
    Qt5Gui.lib
    OrientBasicPrimtives.lib
    OrientBasicContainers.lib
    SAMIngQtSAM.lib
    SAMGui.lib
    SAMCommonCommand.lib
    SAMSessionGuiProxy.lib
    libtcmalloc_minimal.lib
)

# 后构建操作
add_custom_command(TARGET ContainerShipSectionToolset POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DSOURCE=$<TARGET_FILE:ContainerShipSectionToolset>"
        "-DDEST=${LIBS_SAM_ROOT}/Release/FilePlugin/SAM.Pre.ContainerShipSectionToolset.dll"
        -P "${CMAKE_SOURCE_DIR}/cmake/copy_if_possible.cmake"
)
