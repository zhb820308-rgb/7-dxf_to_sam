# Production and tests share these implementations through stable targets.
set(EXAMPLE1_CORE_SOURCES
    DxfData.cpp
    DxfData.h
    DxfImportError.h
    GeometryUtils.cpp
    GeometryUtils.h
    ConversionEngine.cpp
    ConversionEngine.h
    SamData.h
    FeData.cpp
    FeData.h
    FeConversionEngine.cpp
    FeConversionEngine.h
    DxfImportLogger.cpp
    DxfImportLogger.h
)
set(EXAMPLE1_PARSER_SOURCES
    DxfBlockExpansion.cpp
    DxfBlockExpansion.h
    DxfEntityValidation.cpp
    DxfEntityValidation.h
    DxfParser.cpp
    DxfParser.h
    DxfReaderCallbacks.cpp
    DxfReaderCallbacks.h
    DxfTransform.cpp
    DxfTransform.h
)
set(EXAMPLE1_DXF_INPUT_SOURCES
    DxfInputFile.cpp
    DxfInputFile.h
)
set(EXAMPLE1_PLUGIN_SOURCES
    Example1PytModule.cpp
    Example1PytModule.h
    Example1Utils.cpp
    Example1Utils.h
    ImportBuildResult.h
    ImportTransaction.h
    kefKLine.cpp
    kefKLine.h
    PythonFiniteElementBuilder.cpp
    PythonFiniteElementBuilder.h
    SamBuilder.cpp
    SamBuilder.h
    SAMExample1Fragment.cpp
    SAMExample1Fragment.h
)

add_library(Example1Core STATIC ${EXAMPLE1_CORE_SOURCES})
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
    PRIVATE
    ${LIBDXFRW_INCLUDE_DIR}
)
target_link_libraries(Example1DxfParser PUBLIC
    ExampleProjectOptions
    Example1Core
    Example1DxfInput
    Qt5::Core
    "${LIBDXFRW_LIBRARY}"
)
set_target_properties(Example1DxfParser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# dynamic library
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
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    SUFFIX ".pyd"
)

target_link_libraries(Example1
    ExampleProjectOptions
    Example1Core
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
        "${LIBDXFRW_RUNTIME}"
        "$<TARGET_FILE_DIR:Example1>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:spdlog::spdlog>"
        "$<TARGET_FILE_DIR:Example1>"
)
