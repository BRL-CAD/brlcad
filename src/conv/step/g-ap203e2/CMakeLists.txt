link_directories(${CMAKE_BINARY_DIR}/${LIB_DIR})

set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap203e2.exp)
add_definitions(-DAP203e2)

GENERATE_SCHEMA_INPUTS(${STEP_SCHEMA_FILE} gap203e2)

set(gap203e2_SOURCES
  ${express_srcs}
  ../BRLCADWrapper.cpp
  ../g-step/AP203.cpp
  ../g-step/Assembly_Product.cpp
  ../g-step/Comb.cpp
  ../g-step/Default_Geometric_Context.cpp
  ../g-step/G_Objects.cpp
  ../g-step/ON_NurbsCurve.cpp
  ../g-step/ON_NurbsSurface.cpp
  ../g-step/ON_Brep.cpp
  ../g-step/Shape_Definition_Representation.cpp
  ../g-step/Shape_Representation.cpp
  ../g-step/Shape_Representation_Relationship.cpp
  ../g-step/Trees.cpp
  g-ap203e2.cpp
  )

set(gap203e2_LIBS
  libwdb
  librt
  libbrep
  libbn
  libbu
  ${STEPCODE_UTILS_LIBRARY}
  ${STEPCODE_EDITOR_LIBRARY}
  ${STEPCODE_DAI_LIBRARY}
  ${STEPCODE_CORE_LIBRARY}
  ${OPENNURBS_LIBRARIES}
  )

set(AP203_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../step-g
  ${STEPCODE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
  )

BRLCAD_ADDEXEC(g-ap203e2 "${gap203e2_SOURCES}" "${gap203e2_LIBS}" NO_STRICT FOLDER Conv)
target_include_directories(g-ap203e2 BEFORE PRIVATE ${AP203_INCLUDE_DIRS})
add_dependencies(g-ap203e2 step-express-${SCHEMA_NAME}-gap203e2)

if (HIDE_INTERNAL_SYMBOLS)
  set(gap203e2_IMPORTS
    BU_DLL_IMPORTS
    BN_DLL_IMPORTS
    RT_DLL_IMPORTS
    WDB_DLL_IMPORTS
    SC_CORE_DLL_IMPORTS
    SC_DAI_DLL_IMPORTS
    SC_EDITOR_DLL_IMPORTS
    SC_UTILS_DLL_IMPORTS
    )
  set_property(TARGET g-step APPEND PROPERTY COMPILE_DEFINITIONS "${gap203e2_IMPORTS}")
endif (HIDE_INTERNAL_SYMBOLS)

set(IGNORE_FILES
  CMakeLists.txt
  AP203e2.h
  ap203e2.exp
  )
CMAKEFILES(${IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
