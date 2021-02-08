
set(SC_BASE_SOURCES
  sc_memmgr.cc
  sc_trace_fprintf.c
  sc_getopt.cc
  sc_benchmark.cc
  sc_mkdir.c
  path2str.c
  judy/src/judy.c
 )

set(SC_BASE_HDRS
  sc_benchmark.h
  sc_memmgr.h
  sc_getopt.h
  sc_trace_fprintf.h
  sc_mkdir.h
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

if(BUILD_SHARED_LIBS)
  SC_ADDLIB(base SHARED SOURCES ${SC_BASE_SOURCES})
  if(WIN32)
    target_link_libraries(base psapi)
    target_compile_definitions(base PRIVATE SC_BASE_DLL_EXPORTS)
  endif()
endif()

if(BUILD_STATIC_LIBS)
  SC_ADDLIB(base-static STATIC SOURCES ${SC_BASE_SOURCES})
  if(WIN32)
    target_link_libraries(base-static psapi)
  endif()
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

install(FILES ${SC_BASE_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/base)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

