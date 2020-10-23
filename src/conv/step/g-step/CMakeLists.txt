# local includes
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${STEPCODE_INCLUDE_DIRS}
  )

link_directories(${CMAKE_BINARY_DIR}/${LIB_DIR})

set(STEP_SCHEMA_FILE ${CMAKE_SOURCE_DIR}/src/other/ext/stepcode/data/ap203/ap203.exp)
add_definitions(-DAP203)

GENERATE_SCHEMA_INPUTS(${STEP_SCHEMA_FILE} gstep)

set(gstep_SOURCES
  ${express_srcs}
  ../BRLCADWrapper.cpp
  AP203.cpp
  Assembly_Product.cpp
  Comb.cpp
  Default_Geometric_Context.cpp
  G_Objects.cpp
  ON_NurbsCurve.cpp
  ON_NurbsSurface.cpp
  ON_Brep.cpp
  Shape_Definition_Representation.cpp
  Shape_Representation.cpp
  Shape_Representation_Relationship.cpp
  Trees.cpp
  g-step.cpp
  )
CMAKEFILES(${gstep_SOURCES})

if (BRLCAD_ENABLE_STEP)
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
    ${OPENNURBS_LIBRARIES}
    )

  BRLCAD_ADDEXEC(g-step "${gstep_SOURCES}" "${gstep_LIBS}" NO_STRICT)
  add_dependencies(g-step step-express-${SCHEMA_NAME}-gstep)
  set_property(TARGET g-step APPEND PROPERTY INCLUDE_DIRECTORIES "${gstep_SCHEMA_OUT_DIR}")

  if (HIDE_INTERNAL_SYMBOLS)
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
    set_property(TARGET g-step APPEND PROPERTY COMPILE_DEFINITIONS "${gstep_IMPORTS}")
  endif (HIDE_INTERNAL_SYMBOLS)
endif (BRLCAD_ENABLE_STEP)

set(IGNORE_FILES
  CMakeLists.txt
  AP203.h
  Assembly_Product.h
  Comb.h
  Default_Geometric_Context.h
  G_Objects.h
  ON_Brep.h
  Shape_Definition_Representation.h
  Shape_Representation.h
  Shape_Representation_Relationship.h
  Trees.h
  )
CMAKEFILES(${IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
