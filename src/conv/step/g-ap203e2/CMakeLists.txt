set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap203e2.exp)

if(DEFINED ENABLE_STEP_EXPERIMENTAL)
  generate_schema_inputs(${STEP_SCHEMA_FILE} gap203e2)
endif(DEFINED ENABLE_STEP_EXPERIMENTAL)

set(
  gap203e2_SOURCES
  ${gap203e2_express_srcs}
  ../BRLCADWrapper.cpp
  ../STEPString.cpp
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

set(
  gap203e2_LIBS
  libwdb
  librt
  libbrep
  libbn
  libbu
  ${STEPCODE_UTILS_LIBRARY}
  ${STEPCODE_EDITOR_LIBRARY}
  ${STEPCODE_DAI_LIBRARY}
  ${STEPCODE_CORE_LIBRARY}
  OPENNURBS::OPENNURBS
)

set(
  AP203_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${gap203e2_SCHEMA_OUT_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../step-g
  ${STEPCODE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
)

if(DEFINED ENABLE_STEP_EXPERIMENTAL)
  brlcad_addexec(g-ap203e2 "${gap203e2_SOURCES}" "${gap203e2_LIBS}" NO_STRICT FOLDER Conv)
  target_include_directories(g-ap203e2 BEFORE PRIVATE ${AP203_INCLUDE_DIRS})
  target_compile_definitions(g-ap203e2 PRIVATE AP203e2)
  add_dependencies(g-ap203e2 ${gap203e2_SCHEMA_TARGET})
endif(DEFINED ENABLE_STEP_EXPERIMENTAL)

set(IGNORE_FILES ${gap203e2_SOURCES} CMakeLists.txt AP203e2.h ap203e2.exp)
cmakefiles(${IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
