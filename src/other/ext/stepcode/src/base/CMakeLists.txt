set(BASE_SOURCES
  sc_memmgr.cc
  sc_trace_fprintf.c
  sc_getopt.cc
  sc_benchmark.cc
  sc_mkdir.c
  path2str.c
  judy/src/judy.c
 )

set(BASE_HDRS
  sc_benchmark.h
  sc_memmgr.h
  sc_getopt.h
  sc_trace_fprintf.h
  sc_mkdir.h
  sc_nullptr.h
  path2str.h
  judy/src/judy.h
  judy/src/judyLArray.h
  judy/src/judyL2Array.h
  judy/src/judySArray.h
  judy/src/judyS2Array.h
 )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/judy/src
  )

if($CACHE{SC_MEMMGR_ENABLE_CHECKS})
  add_definitions(-DSC_MEMMGR_ENABLE_CHECKS)
endif()

add_library(base-obj OBJECT ${BASE_SOURCES} ${BASE_HDRS})
set_property(TARGET base-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  set_property(TARGET base-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_BASE_DLL_EXPORTS")
endif(MSVC)

add_library(base SHARED $<TARGET_OBJECTS:base-obj>)
if(OPENBSD)
  set_target_properties(base PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(base PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET base APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  target_link_libraries(base psapi)
  set_property(TARGET base APPEND PROPERTY COMPILE_DEFINITIONS "SC_BASE_DLL_EXPORTS")
  set_property(TARGET base APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_BASE_DLL_IMPORTS")
endif(MSVC)
install(TARGETS base
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(BUILD_STATIC_LIBS)
  add_library(base-static STATIC $<TARGET_OBJECTS:base-obj>)
  install(TARGETS base-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif()

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/judy/src")
  message("Judy array source code not found. Downloading (don't sweat, it's public domain)...")
  file(DOWNLOAD "http://github.com/mpictor/judy-template/archive/master.tar.gz"
    "${CMAKE_CURRENT_SOURCE_DIR}/judy.tar.gz" SHOW_PROGRESS)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_SOURCE_DIR}/judy.tar.gz"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
   )
  file(RENAME judy-template-master judy)
  file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/judy.tar.gz")
  message("Judy array source code extracted.")
endif(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/judy/src")

install(FILES ${BASE_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/base)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

