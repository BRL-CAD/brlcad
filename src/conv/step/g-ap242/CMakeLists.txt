set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap242e1.exp)
add_definitions(-DAP242)

if(DEFINED ENABLE_STEP_EXPERIMENTAL)
  generate_schema_inputs(${STEP_SCHEMA_FILE} gap242)
endif(DEFINED ENABLE_STEP_EXPERIMENTAL)

set(
  gap242_SOURCES
  ${express_srcs}
  ../BRLCADWrapper.cpp
  ../g-step/AP203.cpp
  ../g-step/Default_Geometric_Context.cpp
  ../g-step/ON_Brep.cpp
  ../g-step/ON_NurbsCurve.cpp
  ../g-step/ON_NurbsSurface.cpp
  ../g-ap214/Shape_Definition_Representation.cpp
  ../g-step/Shape_Representation.cpp
  ../g-step/Shape_Representation_Relationship.cpp
  g-ap242.cpp
)

set(
  gap242_LIBS
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

set(
  AP242_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../g-step
  ${STEPCODE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
)

if(DEFINED ENABLE_STEP_EXPERIMENTAL)
  brlcad_addexec(g-ap242 "${gap242_SOURCES}" "${gap242_LIBS}" NO_STRICT FOLDER Conv)
  target_include_directories(g-ap242 BEFORE PRIVATE ${AP242_INCLUDE_DIRS})
  add_dependencies(g-ap242 step-express-${SCHEMA_NAME}-gap242)
  set_property(
    SOURCE
      ${CMAKE_CURRENT_BINARY_DIR}/AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF/SdaiAP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF.cc
    APPEND
    PROPERTY COMPILE_FLAGS "/bigobj"
  )
endif(DEFINED ENABLE_STEP_EXPERIMENTAL)

set(IGNORE_FILES ${gap242_SOURCES} CMakeLists.txt AP242.h ap242e1.exp)
cmakefiles(${IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
