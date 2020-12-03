
set(LIBSTEPEDITOR_SRCS
  STEPfile.cc
  STEPfile.inline.cc
  cmdmgr.cc
  SdaiHeaderSchema.cc
  SdaiHeaderSchemaAll.cc
  SdaiHeaderSchemaInit.cc
  SdaiSchemaInit.cc
  )

SET(SC_CLEDITOR_HDRS
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

if($CACHE{SC_BUILD_SHARED_LIBS})
  SC_ADDLIB(stepeditor SHARED SOURCES ${LIBSTEPEDITOR_SRCS} LINK_LIBRARIES stepcore stepdai steputils base)
  if(WIN32)
    target_compile_definitions(stepeditor PRIVATE SC_EDITOR_DLL_EXPORTS)
  endif()
endif()

if($CACHE{SC_BUILD_STATIC_LIBS})
  SC_ADDLIB(stepeditor-static STATIC SOURCES ${LIBSTEPEDITOR_SRCS} LINK_LIBRARIES stepcore-static stepdai-static steputils-static base-static)
endif()

install(FILES ${SC_CLEDITOR_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/cleditor)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

