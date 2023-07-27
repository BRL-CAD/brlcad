include (VCICommon)

include_directories (
  ../..
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# source code directories
set (directories 
  . 
  Decimater
  Dualizer
  Smoother
  Subdivider/Adaptive/Composite
  Subdivider/Uniform/Composite 
  Subdivider/Uniform
  Utils
  VDPM
)

# collect all header and source files
vci_append_files (headers "*.hh" ${directories})
vci_append_files (sources "*.cc" ${directories})

IF(WIN32 AND NOT MINGW)
  vci_append_files (sources "*.c" ${directories})
ENDIF(WIN32 AND NOT MINGW)

# Disable Library installation when not building OpenMesh on its own but as part of another project!
if ( NOT ${CMAKE_PROJECT_NAME} MATCHES "OpenMesh")
  set(VCI_NO_LIBRARY_INSTALL true)
endif()


if (WIN32)

  if ( OPENMESH_BUILD_SHARED )
    add_definitions( -DOPENMESHDLL -DBUILDOPENMESHDLL)
    vci_add_library (OpenMeshTools SHARED ${sources} ${headers})
  else()
    # OpenMesh has no dll exports so we have to build a static library on windows
    vci_add_library (OpenMeshTools STATIC ${sources} ${headers})
  endif()

else ()
  vci_add_library (OpenMeshTools SHAREDANDSTATIC ${sources} ${headers})
  set_target_properties (OpenMeshTools PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
                                                SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR} )
endif ()

target_link_libraries (OpenMeshTools OpenMeshCore)

IF( NOT WIN32 )
  target_link_libraries (OpenMeshToolsStatic OpenMeshCoreStatic)
ENDIF(NOT WIN32)

if ( (${CMAKE_PROJECT_NAME} MATCHES "OpenMesh") AND BUILD_APPS )

  if ( WIN32 )
    if ( NOT "${CMAKE_GENERATOR}" MATCHES "MinGW Makefiles" )
      # let bundle generation depend on target
      add_dependencies (fixbundle OpenMeshTools)
    endif()
  endif()

  # Add tools as dependency before fixbundle 
  if (APPLE)
    # let bundle generation depend on target
    add_dependencies (fixbundle OpenMeshTools)
    add_dependencies (fixbundle OpenMeshToolsStatic)
  endif()

endif()


# Install Header Files (Apple)
if ( NOT VCI_PROJECT_MACOS_BUNDLE AND APPLE )
 FILE(GLOB files_install_Decimater                       "${CMAKE_CURRENT_SOURCE_DIR}/Decimater/*.hh" )
 FILE(GLOB files_install_Dualizer                        "${CMAKE_CURRENT_SOURCE_DIR}/Dualizer/*.hh" )
 FILE(GLOB files_install_KERNEL_OSG                      "${CMAKE_CURRENT_SOURCE_DIR}/Kernel_OSG/*.hh" )
 FILE(GLOB files_install_Smoother                        "${CMAKE_CURRENT_SOURCE_DIR}/Smoother/*.hh" )
 FILE(GLOB files_install_Subdivider_Adaptive             "${CMAKE_CURRENT_SOURCE_DIR}/Subdivider/Adaptive/Composite/*.hh" )
 FILE(GLOB files_install_Subdivider_Uniform              "${CMAKE_CURRENT_SOURCE_DIR}/Subdivider/Uniform/*.hh" )
 FILE(GLOB files_install_Subdivider_Uniform_Composite    "${CMAKE_CURRENT_SOURCE_DIR}/Subdivider/Uniform/Composite/*.hh" )
 FILE(GLOB files_install_Utils                           "${CMAKE_CURRENT_SOURCE_DIR}/Utils/*.hh" "${CMAKE_CURRENT_SOURCE_DIR}/Utils/getopt.h" )
 FILE(GLOB files_install_VDPM                            "${CMAKE_CURRENT_SOURCE_DIR}/VDPM/*.hh" )
 INSTALL(FILES ${files_install_Decimater}                       DESTINATION include/OpenMesh/Tools/Decimater )
 INSTALL(FILES ${files_install_Dualizer}                        DESTINATION include/OpenMesh/Tools/Dualizer )
 INSTALL(FILES ${files_install_KERNEL_OSG}                      DESTINATION include/OpenMesh/Tools/Kernel_OSG )
 INSTALL(FILES ${files_install_Smoother}                        DESTINATION include/OpenMesh/Tools/Smoother )
 INSTALL(FILES ${files_install_Subdivider_Adaptive}             DESTINATION include/OpenMesh/Tools/Subdivider/Adaptive/Composite )
 INSTALL(FILES ${files_install_Subdivider_Uniform}              DESTINATION include/OpenMesh/Tools/Subdivider/Uniform )
 INSTALL(FILES ${files_install_Subdivider_Uniform_Composite}    DESTINATION include/OpenMesh/Tools/Subdivider/Uniform/Composite )
 INSTALL(FILES ${files_install_Utils}                           DESTINATION include/OpenMesh/Tools/Utils )
 INSTALL(FILES ${files_install_VDPM}                            DESTINATION include/OpenMesh/Tools/VDPM )
endif()


# Only install if the project name matches OpenMesh.
if (${CMAKE_PROJECT_NAME} MATCHES "OpenMesh")
  set (OPENMESH_NO_INSTALL_HEADERS FALSE CACHE BOOL "Should OpenMesh skip installing headers?")
else()
  set (OPENMESH_NO_INSTALL_HEADERS TRUE CACHE BOOL "Should OpenMesh skip installing headers?")
endif()

if (NOT APPLE AND NOT ${OPENMESH_NO_INSTALL_HEADERS})

# Install Header Files
install(DIRECTORY .
        DESTINATION include/OpenMesh/Tools
        FILES_MATCHING
        PATTERN "*.hh"
        PATTERN "CVS" EXCLUDE
        PATTERN ".svn" EXCLUDE
        PATTERN "tmp" EXCLUDE
        PATTERN "Templates" EXCLUDE
        PATTERN "Debian*" EXCLUDE)

#install the config file
install(FILES Utils/getopt.h DESTINATION include/OpenMesh/Tools/Utils)

endif ()

target_include_directories(OpenMeshTools PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
  $<INSTALL_INTERFACE:include>)

install(TARGETS OpenMeshTools EXPORT OpenMeshConfig
  ARCHIVE DESTINATION ${VCI_PROJECT_LIBDIR}
  LIBRARY DESTINATION ${VCI_PROJECT_LIBDIR}
  RUNTIME DESTINATION ${VCI_PROJECT_BINDIR})

