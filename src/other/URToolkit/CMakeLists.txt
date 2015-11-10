IF(COMMAND CMAKE_POLICY)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
	  CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
ENDIF(COMMAND CMAKE_POLICY)

include(CheckIncludeFiles)
include(CheckFunctionExists)

check_function_exists(gethostname HAVE_GETHOSTNAME)
check_include_files (unistd.h HAVE_UNISTD_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

IF(NOT MSVC)
  MACRO(URT_EXEC execname srcs)
    STRING(REGEX REPLACE " " ";" srcslist "${srcs};config.h.in")
    add_executable(${execname} ${srcslist})
    target_link_libraries(${execname} ${UTAHRLE_LIBRARY})
    INSTALL(TARGETS ${execname} RUNTIME DESTINATION bin)
  ENDMACRO()

  ADD_SUBDIRECTORY(cnv)
  ADD_SUBDIRECTORY(man)
  ADD_SUBDIRECTORY(tools)
ENDIF(NOT MSVC)
