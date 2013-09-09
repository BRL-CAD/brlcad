# local includes
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

GENERATE_SCHEMA_INPUTS(${STEP_SCHEMA_FILE} gstep)

set(gstep_SOURCES
  ${express_srcs}
  ../BRLCADWrapper.cpp
  G_STEP_Util.cpp
  ON_NurbsCurve.cpp
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
add_dependencies(g-step step-express-${SCHEMA_NAME}-gstep)

if(MSVC)
  set(gstep_IMPORTS
    BU_DLL_IMPORTS
    BN_DLL_IMPORTS
    RT_DLL_IMPORTS
    WDB_DLL_IMPORTS
    SC_CORE_DLL_IMPORTS
    SC_DAI_DLL_IMPORTS
    SC_EDITOR_DLL_IMPORTS
    SC_UTILS_DLL_IMPORTS
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
