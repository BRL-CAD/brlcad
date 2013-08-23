# local includes
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# The fedex-plus files are shared between step-g and g-step, but
# CMake currently requires that the custom command generating
# source files used in a target build be in the same CMakeLists.txt
# file as the target definition.  Hence, we need to repeat the
# add_custom command definition for each converter that needs the
# fedex output, even though they're all using the same outputs.
# The top-level fedex-plus STEP custom target is used to make sure
# the files expected by this target definition are already in
# place.  That fedex-plus target is added as a dependency to the
# current build target. Consequently, the only purpose of this
# custom command definition is to allow files that don't exist
# at configure time to be part of the target sources list.
add_custom_command(OUTPUT ${FEDEX_OUTPUT}
  COMMAND ${FEDEX_PLUS_EXEC} ARGS ${SCHEMA_FILE}
  DEPENDS ${SCHEMA_FILE} ${FEDEX_PLUS_EXECUTABLE_TARGET}
  WORKING_DIRECTORY ${SCHEMA_OUT_DIR}
  COMMENT "Generating C++ code to express ${SCHEMA_NAME}..."
  VERBATIM)

set(gstep_SOURCES
  ${fedex_srcs}
  ../BRLCADWrapper.cpp
  ON_Brep.cpp
  g-step.cpp
  )

set(gstep_LIBS
  libwdb
  librt
  libbrep
  libbn
  libbu
  steputils
  stepeditor
  stepdai
  stepcore
  ${OPENNURBS_LIBRARY}
  )
list(APPEND gstep_LIBS ${gstep_LIBS})

#add_definitions(-DAP203e2)

BRLCAD_ADDEXEC(g-step "${gstep_SOURCES}" "${gstep_LIBS}" NO_STRICT)
add_dependencies(g-step step-fedex-${SCHEMA_NAME})

if(MSVC)
  set(gstep_IMPORTS
    BU_DLL_IMPORTS
    RT_DLL_IMPORTS
    WDB_DLL_IMPORTS
    SCL_CORE_DLL_IMPORTS
    SCL_DAI_DLL_IMPORTS
    SCL_EDITOR_DLL_IMPORTS
    SCL_UTILS_DLL_IMPORTS
    )
  list(APPEND gstep_IMPORTS ${gstep_IMPORTS})
  set_target_properties(g-step PROPERTIES COMPILE_DEFINITIONS "${gstep_IMPORTS}")
endif(MSVC)

CMAKEFILES(ON_Brep.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
