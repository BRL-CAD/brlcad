
set(STEPEDITOR_SRCS
  STEPfile.cc
  STEPfile.inline.cc
  cmdmgr.cc
  SdaiHeaderSchema.cc
  SdaiHeaderSchemaAll.cc
  SdaiHeaderSchemaInit.cc
  SdaiSchemaInit.cc
  )

SET(STEPEDITOR_HDRS
  STEPfile.h
  cmdmgr.h
  editordefines.h
  SdaiHeaderSchema.h
  SdaiHeaderSchemaClasses.h
  SdaiSchemaInit.h
  seeinfodefault.h
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SC_SOURCE_DIR}/src/base
  ${SC_SOURCE_DIR}/src/cldai
  ${SC_SOURCE_DIR}/src/clstepcore
  ${SC_SOURCE_DIR}/src/clutils
  )

add_library(stepeditor-obj OBJECT ${STEPEDITOR_SRCS})
set_property(TARGET stepeditor-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
add_dependencies(stepeditor-obj base)
if(MSVC)
  set_property(TARGET stepeditor-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_EDITOR_DLL_EXPORTS")
endif(MSVC)

add_library(stepeditor SHARED $<TARGET_OBJECTS:stepeditor-obj>)
target_link_libraries(stepeditor steputils stepdai base)
if(OPENBSD)
  set_target_properties(stepeditor PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(stepeditor PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET stepeditor APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  set_property(TARGET stepeditor APPEND PROPERTY COMPILE_DEFINITIONS "SC_EDITOR_DLL_EXPORTS")
  set_property(TARGET stepeditor APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_EDITOR_DLL_IMPORTS")
endif(MSVC)
install(TARGETS stepeditor
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(stepeditor-static STATIC $<TARGET_OBJECTS:stepeditor-obj>)
  install(TARGETS stepeditor-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

install(FILES ${STEPEDITOR_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/cleditor)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

