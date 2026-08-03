# Production and tests share these implementations through stable targets.
# 文件分组反映架构边界，不只是为了整理列表：Core 不直接依赖 SAM GUI，Parser 把
# libdxfrw 隔离出来，Plugin 才链接庞大的 SAM SDK。测试可以只链接前两层。
set(EXAMPLE1_COMMON_CORE_SOURCES
    DxfData.cpp
    DxfData.h
    GeometryUtils.cpp
    GeometryUtils.h
    DxfImportRuntime.cpp
    DxfImportRuntime.h
)
set(EXAMPLE1_SKETCH_CORE_SOURCES
    SketchConversion.cpp
    SketchConversion.h
)
set(EXAMPLE1_FE_CORE_SOURCES
    FeConversion.cpp
    FeConversion.h
)
set(EXAMPLE1_PARSER_SOURCES
    DxfBlockExpansion.cpp
    DxfBlockExpansion.h
    DxfParser.cpp
    DxfParser.h
)
set(EXAMPLE1_DXF_INPUT_SOURCES
    DxfInputFile.cpp
    DxfInputFile.h
)
set(EXAMPLE1_PLUGIN_SOURCES
    DxfImportApplication.cpp
    DxfImportApplication.h
    SketchImport.cpp
    SketchImport.h
    FeImport.cpp
    FeImport.h
    Example1Module.cpp
    Example1Module.h
)

# STATIC 生成只供链接的静态库，不会被 SAM 单独加载。
add_library(Example1Core STATIC ${EXAMPLE1_COMMON_CORE_SOURCES})
target_include_directories(Example1Core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${SAM_OCCT_INCLUDE_DIR}
)
target_link_directories(Example1Core PUBLIC ${SAM_OCCT_LIB_DIR})
target_link_libraries(Example1Core PUBLIC
    ExampleProjectOptions
    Qt5::Core
    spdlog::spdlog
    TKernel
    TKMath
    TKG2d
    TKG3d
    TKGeomBase
    TKGeomAlgo
)
set_target_properties(Example1Core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Sketch 和 FE 各自拥有独立、可单测的纯转换库。二者只共享 DXF 数据与几何工具，
# 不互相包含对方的中间数据，也不在这里链接庞大的 SAM SDK。
add_library(Example1SketchCore STATIC ${EXAMPLE1_SKETCH_CORE_SOURCES})
target_include_directories(Example1SketchCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Example1SketchCore PUBLIC Example1Core)
set_target_properties(Example1SketchCore PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_library(Example1FeCore STATIC ${EXAMPLE1_FE_CORE_SOURCES})
target_include_directories(Example1FeCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Example1FeCore PUBLIC Example1Core)
set_target_properties(Example1FeCore PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Unicode path adaptation is shared by the parser and the Toolset layer scan
# without pulling the conversion core into the UI plugin.
add_library(Example1DxfInput STATIC ${EXAMPLE1_DXF_INPUT_SOURCES})
target_include_directories(Example1DxfInput PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Example1DxfInput PUBLIC
    ExampleProjectOptions
    Qt5::Core
)
set_target_properties(Example1DxfInput PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Parser has an additional libdxfrw boundary, so keep it separate from the
# dependency-light conversion core.
add_library(Example1DxfParser STATIC ${EXAMPLE1_PARSER_SOURCES})
target_include_directories(Example1DxfParser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(Example1DxfParser PUBLIC
    ExampleProjectOptions
    Example1Core
    Example1DxfInput
    Qt5::Core
    libdxfrw::dxfrw
)
set_target_properties(Example1DxfParser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# SHARED 先生成 Windows 动态库；下面把后缀改成 .pyd，使 SAM 内嵌 Python 把它
# 当作原生扩展模块加载。它本质仍是符合 Windows PE/COFF 格式的动态链接库。
add_library(Example1 SHARED ${EXAMPLE1_PLUGIN_SOURCES})
target_include_directories(Example1 PRIVATE
    ${LIBS_PYTHON_ROOT}/include
    ${LIBS_OC_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include/guiProject
)
target_link_directories(Example1 PRIVATE
    ${LIBS_PYTHON_ROOT}/libs
    ${LIBS_OC_ROOT}/lib
    ${LIBS_SAMSDK_ROOT}/libs
    ${PROJECT_SOURCE_DIR}/lib
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
)
target_compile_definitions(Example1 PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NON_CONFORMING_SWPRINTFS
    UNICODE
    _UNICODE
)

set_target_properties(Example1 PROPERTIES
	# AUTOMOC 让 CMake 自动运行 Qt 元对象编译器；CXX_STANDARD 固定 C++17；
	# SUFFIX 决定最后文件名为 Example1.pyd 而不是 Example1.dll。
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    SUFFIX ".pyd"
)

target_link_libraries(Example1
    ExampleProjectOptions
    Example1Core
    Example1SketchCore
    Example1FeCore
    Example1DxfParser
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    OrientBasicContainers
    OrientBasicUtilities
    OrientBasicCoreUtilities
    OrientBasicPrimtives
    OrientPythonInterfaces
    SAMPythonUtilities
    SAMPythonModuleConfiguration
    SAMCommonCommand
    SAMKernelAttributesInterfaceObjects
    SAMSessionCommonDefs
    SAMSceneGraph
    SAMPrimaryObjects
    SAMKernel
    SAMGeometryCoreModule
    SAMGeometryComponent
    SAMParametricModelingBase
    SAMParametricModeling
    SAMKernelComponents
    SAMKernelBasicAttributesInterfaceObjects
    SAMCoreAttributes
    SAMCommonAttributes
    FESession
    SAMModelDatabase
    SAMIngQtSAM
    SAMGui
    SAMSessionGuiProxy
    SAMVagVwrModGui
    SAMAttributesGui
    SAMAttributesProxies
    SAMCommonModuleDefs
    SAMCommonInteractionDef
    SAMMeshCoreModule
    SAMMeshInterfaceObjects
    SAMCoreMeshDefs
    SAMVisualCommon
    SAMOdbCoreResults
    SAMInbQtSAMCom
    SAMAnimationController
    SAMDatabaseManager
    SAMHdf5Data
    SAMHdf5Interface
    SAMMathUtils
    SAMOdbInterface
    SAMOdbPythonInterface
    SAMInputFileWriter
    SAMInputFileWriterImpl
    FEHDF5Data
    FEKernelSettings
    FEMathUtils
    FEOdbData
    OTLGraphicsVisual
    AutoSet
    customKernel
    textReprInternal
)

# Copy third-party runtime DLLs beside Example1.pyd after build.
add_custom_command(TARGET Example1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:libdxfrw::dxfrw>"
        "$<TARGET_FILE_DIR:Example1>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:spdlog::spdlog>"
        "$<TARGET_FILE_DIR:Example1>"
)
