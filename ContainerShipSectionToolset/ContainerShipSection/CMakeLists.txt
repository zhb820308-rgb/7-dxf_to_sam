# 自动为QObject派生类生成moc文件
set(CMAKE_AUTOMOC ON)
# 将当前目录加入头文件搜索路径
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 包含目录
include_directories(
    ${LIBS_QT5_ROOT}/include
    ${LIBS_QT5_ROOT}/include/QtCore
    ${LIBS_PYTHON_ROOT}/include
    ${LIBS_OC_ROOT}/include
    ${LIBS_FEASOFT_ROOT}/include
    ${LIBS_FEASOFT_ROOT}/include/guiProject
)

# 链接目录
link_directories(
    ${LIBS_QT5_ROOT}/lib
    ${LIBS_PYTHON_ROOT}/libs
    ${LIBS_OC_ROOT}/lib
    ${LIBS_FEASOFT_ROOT}/libs
    ${PROJECT_SOURCE_DIR}/lib
    ${LIBS_TCMALLOC_ROOT}
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
)

# 源文件收集
file(GLOB TARGET_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    )

# Qt处理
set(HEADERS
)
QT5_WRAP_CPP(HEADERS_MOC ${HEADERS})
QT5_ADD_RESOURCES(RESOURCES_RCC ${RESOURCES})
source_group("Generated Files" FILES
    ${HEADERS_MOC}
)

# 创建动态库
add_library(ContainerShipSection SHARED
    ${TARGET_SRC}
    ${HEADERS_MOC}
    ${RESOURCES_RCC}
)

# 编译器设置
set_property(TARGET ContainerShipSection
    PROPERTY COMPILE_DEFINITIONS
    _CRT_SECURE_NO_WARNINGS
    _CRT_NON_CONFORMING_SWPRINTFS
)

# 链接器标志（tcmalloc）
set_target_properties(ContainerShipSection PROPERTIES
    LINK_FLAGS /INCLUDE:"__tcmalloc"
)

# 宏定义
add_definitions(
    -DUNICODE
    -D_UNICODE
)

# Python扩展特殊设置
set_target_properties(ContainerShipSection PROPERTIES
    SUFFIX ".pyd")

# 链接库
target_link_libraries(ContainerShipSection
    Qt5Core
    OrientBasicContainers
    OrientBasicUtilities
    OrientBasicCoreUtilities
    OrientBasicPrimtives
    OrientPythonInterfaces
    SAMPythonModuleConfiguration
    SAMCommonCommand
	SAMParametricModelingBase
    SAMKernelAttributesInterfaceObjects
    SAMSessionCommonDefs
    SAMSceneGraph
    SAMPrimaryObjects
    SAMKernel
    libtcmalloc_minimal

    SAMCoreMeshDefs
    SAMMeshInterfaceObjects
    SAMModelDatabase
    FEMathUtils
    SAMGeometryCoreModule
    SAMKernelAttributesInterfaceObjects
    SAMGeometryComponent
    SAMPythonUtilities
)
