set(STEPUTILS_SRCS
  Str.cc
  dirobj.cc
  gennode.cc
  gennodelist.cc
  gennodearray.cc
  sc_hash.cc
  errordesc.cc
  )

set(STEPUTILS_HDRS
  dirobj.h
  errordesc.h
  gennodearray.h
  gennode.h
  gennodelist.h
  sc_hash.h
  Str.h
  )

include_directories(
  ${SC_SOURCE_DIR}/src/base
  ${SC_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_library(steputils-obj OBJECT ${STEPUTILS_SRCS})
set_property(TARGET steputils-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
add_dependencies(steputils-obj base)
if(MSVC)
  set_property(TARGET steputils-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_UTILS_DLL_EXPORTS")
endif(MSVC)

add_library(steputils SHARED $<TARGET_OBJECTS:steputils-obj>)
target_link_libraries(steputils base)
if(OPENBSD)
  set_target_properties(steputils PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(steputils PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET steputils APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  target_link_libraries(steputils shlwapi)
  set_property(TARGET steputils APPEND PROPERTY COMPILE_DEFINITIONS "SC_UTILS_DLL_EXPORTS")
  set_property(TARGET steputils APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_UTILS_DLL_IMPORTS")
endif(MSVC)
install(TARGETS steputils
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(steputils-static STATIC $<TARGET_OBJECTS:steputils-obj>)
  install(TARGETS steputils-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

install(FILES ${STEPUTILS_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/clutils)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

