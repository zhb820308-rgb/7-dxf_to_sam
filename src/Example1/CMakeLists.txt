set(CMAKE_AUTOMOC ON) 
set(CMAKE_INCLUDE_CURRENT_DIR ON) 

# directories
include_directories(
    ${LIBS_QT5_ROOT}/include
    ${LIBS_QT5_ROOT}/include/QtCore
    ${LIBS_QT5_ROOT}/include/QtGui
    ${LIBS_QT5_ROOT}/include/QtWidgets
    ${LIBS_PYTHON_ROOT}/include
    ${LIBS_OC_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include/guiProject
    ${LIBDXFRW_INCLUDE_DIR}
    ${SAM_OCCT_INCLUDE_DIR}
)
  
link_directories(
    ${LIBS_QT5_ROOT}/lib
    ${LIBS_PYTHON_ROOT}/libs
    ${LIBS_OC_ROOT}/lib
    ${LIBS_SAMSDK_ROOT}/libs
    ${PROJECT_SOURCE_DIR}/lib
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    ${SAM_OCCT_LIB_DIR}
)

# source list
file(GLOB TARGET_SRC
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.T"
    )

# Production and tests share these implementations through stable targets.
# Keep them out of the plugin's glob so each source is compiled only once.
set(EXAMPLE1_CORE_SOURCES
    DxfData.cpp
    GeometryUtils.cpp
    ConversionEngine.cpp
    FeData.cpp
    FeConversionEngine.cpp
    DxfImportLogger.cpp
)
set(EXAMPLE1_PARSER_SOURCES
    DxfParser.cpp
)
list(TRANSFORM EXAMPLE1_CORE_SOURCES
    PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
    OUTPUT_VARIABLE EXAMPLE1_CORE_SOURCE_PATHS
)
list(TRANSFORM EXAMPLE1_PARSER_SOURCES
    PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
    OUTPUT_VARIABLE EXAMPLE1_PARSER_SOURCE_PATHS
)
list(REMOVE_ITEM TARGET_SRC
    ${EXAMPLE1_CORE_SOURCE_PATHS}
    ${EXAMPLE1_PARSER_SOURCE_PATHS}
)

add_library(Example1Core STATIC ${EXAMPLE1_CORE_SOURCES})
target_include_directories(Example1Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SAM_OCCT_INCLUDE_DIR}
)
target_link_libraries(Example1Core PUBLIC
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
)

# Parser has an additional libdxfrw boundary, so keep it separate from the
# dependency-light conversion core.
add_library(Example1DxfParser STATIC ${EXAMPLE1_PARSER_SOURCES})
target_include_directories(Example1DxfParser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBDXFRW_INCLUDE_DIR}
)
target_link_libraries(Example1DxfParser PUBLIC
    Example1Core
    Qt5::Core
    "${LIBDXFRW_LIBRARY}"
)
set_target_properties(Example1DxfParser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
     
# Moc Qt headers
set(HEADERS
)

QT5_WRAP_CPP(HEADERS_MOC ${HEADERS})
QT5_ADD_RESOURCES(RESOUCES_RCC ${RESOUCES})


# add filters
source_group("Generated Files" FILES
    ${HEADERS_MOC}
)

# dynamic library
add_library(Example1 SHARED
    ${TARGET_SRC}
    ${HEADERS_MOC}
    ${RESOUCES_RCC}
)

# compiler settings
set_property(TARGET Example1
    PROPERTY COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS _CRT_NON_CONFORMING_SWPRINTFS
)

set_target_properties(Example1 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# set_target_properties(Example1 PROPERTIES LINK_FLAGS /INCLUDE:"__tcmalloc")  # disabled tcmalloc temporarily



add_definitions(
    -DUNICODE
    -D_UNICODE
)

# linker settings
set_target_properties(Example1 PROPERTIES
    SUFFIX ".pyd")

target_link_libraries(Example1
    Example1Core
    Example1DxfParser
    Qt5Core
    Qt5Gui
    Qt5Widgets
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
    "${LIBDXFRW_LIBRARY}"
    spdlog::spdlog
    TKernel
    TKMath
    TKG2d
    TKG3d
    TKGeomBase
    TKGeomAlgo
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
