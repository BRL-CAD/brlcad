
set(clLazyFile_SRCS
  lazyDataSectionReader.cc
  lazyFileReader.cc
  lazyInstMgr.cc
  p21HeaderSectionReader.cc
  sectionReader.cc
  lazyP21DataSectionReader.cc
  )

set(clLazyFile_HDRS
  headerSectionReader.h
  lazyFileReader.h
  lazyP21DataSectionReader.h
  p21HeaderSectionReader.h
  lazyDataSectionReader.h
  lazyInstMgr.h
  lazyTypes.h
  sectionReader.h
  instMgrHelper.h
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SC_SOURCE_DIR}/src/cleditor
  ${SC_SOURCE_DIR}/src/cldai
  ${SC_SOURCE_DIR}/src/clstepcore
  ${SC_SOURCE_DIR}/src/clutils
  ${SC_SOURCE_DIR}/src/base
  ${SC_SOURCE_DIR}/src/base/judy/src
  )

add_library(steplazyfile-obj OBJECT ${clLazyFile_SRCS})
set_property(TARGET steplazyfile-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
add_dependencies(steplazyfile-obj base)
if(MSVC)
  set_property(TARGET steplazyfile-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_LAZYFILE_DLL_EXPORTS")
endif(MSVC)

add_library(steplazyfile SHARED $<TARGET_OBJECTS:steplazyfile-obj>)
target_link_libraries(steplazyfile stepcore stepdai steputils base stepeditor)
if(OPENBSD)
  set_target_properties(steplazyfile PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(steplazyfile PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET steplazyfile APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  set_property(TARGET steplazyfile APPEND PROPERTY COMPILE_DEFINITIONS "SC_LAZYFILE_DLL_EXPORTS")
  set_property(TARGET steplazyfile APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_LAZYFILE_DLL_IMPORTS")
endif(MSVC)
install(TARGETS steplazyfile
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(steplazyfile-static STATIC $<TARGET_OBJECTS:steplazyfile-obj>)
  install(TARGETS steplazyfile-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

install(FILES ${clLazyFile_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/cllazyfile)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

