
set(LIBSTEPUTILS_SRCS
  Str.cc
  dirobj.cc
  gennode.cc
  gennodelist.cc
  gennodearray.cc
  sc_hash.cc
  errordesc.cc
  )

set(SC_CLUTILS_HDRS
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

if(BUILD_SHARED_LIBS)
  SC_ADDLIB(steputils SHARED SOURCES ${LIBSTEPUTILS_SRCS} LINK_LIBRARIES base)
  if(WIN32)
    target_compile_definitions(steputils PRIVATE SC_UTILS_DLL_EXPORTS)
    target_link_libraries(steputils shlwapi)
  endif()
endif()

if(BUILD_STATIC_LIBS)
  SC_ADDLIB(steputils-static STATIC SOURCES ${LIBSTEPUTILS_SRCS} LINK_LIBRARIES base-static)
  if(WIN32)
    target_link_libraries(steputils-static shlwapi)
  endif()
endif()

install(FILES ${SC_CLUTILS_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/clutils)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

