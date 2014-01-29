# local includes
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(STEP_SCHEMA_FILE ${STEPCODE_DIR}/data/ap203/ap203.exp)
add_definitions(-DAP203)

GENERATE_SCHEMA_INPUTS(${STEP_SCHEMA_FILE} gstep)

set(gstep_SOURCES
  ${express_srcs}
  ../BRLCADWrapper.cpp
  AP203.cpp
  Assembly_Product.cpp
  Comb.cpp
  Default_Geometric_Context.cpp
  ON_NurbsCurve.cpp
  ON_NurbsSurface.cpp
  ON_Brep.cpp
  Shape_Definition_Representation.cpp
  Shape_Representation.cpp
  Shape_Representation_Relationship.cpp
  Trees.cpp
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

set(IGNORE_FILES
  AP203.h
  Assembly_Product.h
  Comb.h
  Default_Geometric_Context.h
  ON_Brep.h
  Shape_Definition_Representation.h
  Shape_Representation.h
  Shape_Representation_Relationship.h
  list_elements.cpp
  Trees.h
  )
CMAKEFILES(${IGNORE_FILES})

set(list_elements_srcs ${express_srcs} list_elements.cpp)
BRLCAD_ADDEXEC(list_elements "${list_elements_srcs}" "${gstep_LIBS}" NO_STRICT NO_INSTALL)
if(MSVC)
  set_target_properties(list_elements PROPERTIES COMPILE_DEFINITIONS "${gstep_IMPORTS}")
endif(MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
