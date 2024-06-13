set(IFC_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/IFC4.exp)

if (DEFINED ENABLE_STEP_EXPERIMENTAL)
  GENERATE_SCHEMA_INPUTS(${IFC_SCHEMA_FILE} ifcg)
endif (DEFINED ENABLE_STEP_EXPERIMENTAL)

set(ifcg_SOURCES
  ${express_srcs}
  IFCWrapper.cpp
  ifc-g.cpp
  )

set(ifcg_LIBS
  libwdb
  librt
  libbu
  libbrep
  ${STEPCODE_UTILS_LIBRARY}
  ${STEPCODE_EDITOR_LIBRARY}
  ${STEPCODE_DAI_LIBRARY}
  ${STEPCODE_CORE_LIBRARY}
  ${OPENNURBS_LIBRARIES}
  )

set(IFC_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${STEPCODE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
  )

if (DEFINED ENABLE_STEP_EXPERIMENTAL)
  BRLCAD_ADDEXEC(ifc-g "${ifcg_SOURCES}" "${ifcg_LIBS}" NO_STRICT FOLDER Conv)
  add_dependencies(ifc-g ifc-express-${SCHEMA_NAME}-ifcg)
  target_include_directories(ifc-g SYSTEM BEFORE PRIVATE ${IFC_INCLUDE_DIRS})
endif (DEFINED ENABLE_STEP_EXPERIMENTAL)

CMAKEFILES(
  ${ifcg_SOURCES}
  CMakeLists.txt
  IFC4.exp
  IFCWrapper.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
