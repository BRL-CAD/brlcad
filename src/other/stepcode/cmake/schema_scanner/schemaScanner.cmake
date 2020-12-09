# this file should be included from data/CMakeLists.txt
#
# at configure time, this builds a small program
# which will parse express schemas to determine
# what files exp2cxx will create for that schema.
#
# The SCHEMA_CMLIST macro is to be used to run this
# program. It will set variables for schema name(s),
# headers, and implementation files.

# in a unity build, many small .cc files are #included to create a few large translation units
# this makes compilation faster, but sometimes runs into compiler limitations
if(NOT DEFINED SC_UNITY_BUILD)
  message( STATUS "Assuming compiler is capable of unity build. (SC_UNITY_BUILD=TRUE)")
  set(SC_UNITY_BUILD TRUE)
  message( STATUS "Override by setting SC_UNITY_BUILD; TRUE will result in faster build times but *huge* translation units and higher memory use in compilation.")
else(NOT DEFINED SC_UNITY_BUILD)
  message( STATUS "Respecting user-defined SC_UNITY_BUILD value of ${SC_UNITY_BUILD}.")
endif(NOT DEFINED SC_UNITY_BUILD)


# --- variables ---
# SC_ROOT: SC root dir
# SC_BUILDDIR: SC build dir, so generated headers can be found
# SCANNER_SRC_DIR: dir this file is in
# SCANNER_OUT_DIR: location of binary, same dir as SC uses
# SCANNER_BUILD_DIR: location scanner is built

set(SCANNER_SRC_DIR ${SC_CMAKE_DIR}/schema_scanner)
set(SCANNER_BUILD_DIR ${SC_BINARY_DIR}/schema_scanner CACHE INTERNAL "location for scanner build, config files (copied schemas)")
set(SCANNER_OUT_DIR ${SC_BINARY_DIR}/bin CACHE INTERNAL "location for schema_scanner executable")

#write a cmake file for the cache. the alternative is a very long
# command line - and the command line can't have newlines in it
set(initial_scanner_cache ${SCANNER_BUILD_DIR}/initial_scanner_cache.cmake)
file(WRITE ${initial_scanner_cache} "
set(SC_ROOT \"${SC_SOURCE_DIR}\" CACHE STRING \"root dir\")
set(SC_BUILDDIR \"${SC_BINARY_DIR}\" CACHE PATH \"build dir\")
set(CALLED_FROM \"STEPCODE_CMAKELISTS\" CACHE STRING \"verification\")
set(CMAKE_BUILD_TYPE \"Debug\" CACHE STRING \"build type\")
set(CMAKE_C_COMPILER \"${CMAKE_C_COMPILER}\" CACHE STRING \"compiler\")
set(CMAKE_CXX_COMPILER \"${CMAKE_CXX_COMPILER}\" CACHE STRING \"compiler\")
")

message( STATUS "Compiling schema scanner...")

execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${SC_BINARY_DIR}/schemas)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${SCANNER_BUILD_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} -C ${initial_scanner_cache} ${SCANNER_SRC_DIR} -G ${CMAKE_GENERATOR}
                 WORKING_DIRECTORY ${SCANNER_BUILD_DIR}
                 TIMEOUT 60
                 OUTPUT_VARIABLE _ss_config_out
                 RESULT_VARIABLE _ss_config_stat
                 ERROR_VARIABLE _ss_config_err
               )
if(NOT ${_ss_config_stat} STREQUAL "0")
  message(FATAL_ERROR "Scanner config status: ${_ss_config_stat}. stdout:\n${_ss_config_out}\nstderr:\n${_ss_config_err}")
endif(NOT ${_ss_config_stat} STREQUAL "0")
execute_process(COMMAND ${CMAKE_COMMAND} --build ${SCANNER_BUILD_DIR} --config Debug --clean-first
                 WORKING_DIRECTORY ${SCANNER_BUILD_DIR}
                 TIMEOUT 120 # should take far less than 2m
                 OUTPUT_VARIABLE _ss_build_out
                 RESULT_VARIABLE _ss_build_stat
                 ERROR_VARIABLE _ss_build_err
              )
if(NOT ${_ss_build_stat} STREQUAL "0")
  message(FATAL_ERROR "Scanner build status: ${_ss_build_stat}. stdout:\n${_ss_build_out}\nstderr:\n${_ss_build_err}")
endif(NOT ${_ss_build_stat} STREQUAL "0")

message( STATUS "Schema scanner built. Running it...")

# not sure if it makes sense to install this or not...
if(WIN32)
	install(PROGRAMS ${SCANNER_OUT_DIR}/schema_scanner.exe DESTINATION ${BIN_INSTALL_DIR})
else(WIN32)
	install(PROGRAMS ${SCANNER_OUT_DIR}/schema_scanner DESTINATION ${BIN_INSTALL_DIR})
endif(WIN32)

# macro SCHEMA_CMLIST
# runs the schema scanner on one express file, creating a CMakeLists.txt file for each schema found. Those files are added via add_subdirectory().
#
# SCHEMA_FILE - path to the schema
# TODO should we have a result variable to return schema name(s) found?
macro(SCHEMA_CMLIST SCHEMA_FILE)
  execute_process(COMMAND ${SCANNER_OUT_DIR}/schema_scanner ${SCHEMA_FILE}
                   WORKING_DIRECTORY ${SC_BINARY_DIR}/schemas
                   RESULT_VARIABLE _ss_stat
                   OUTPUT_VARIABLE _ss_out
                   ERROR_VARIABLE _ss_err
                )
  if(NOT "${_ss_stat}" STREQUAL "0")
    #check size of output, put in file if large?
    message(FATAL_ERROR "Schema scan for '${SCHEMA_FILE}'\nexited with error code '${_ss_stat}'\nstdout:\n${_ss_out}\nstderr:\n${_ss_err}\n")
  endif(NOT "${_ss_stat}" STREQUAL "0")
  # scanner output format: each line contains an absolute path. each path is a dir containing a CMakeLists for one schema
  # there will usually be a single line of output, but it is not illegal for multiple schemas to exist in one .exp file
  string(STRIP "${_ss_out}" _ss_stripped)
  string(REGEX REPLACE "\\\n" ";" _list ${_ss_stripped})
  foreach(_dir ${_list})
    add_subdirectory(${_dir} ${_dir}) #specify source and binary dirs as the same
  endforeach(_dir ${_ss_out})
  # configure_file forces cmake to run again if the schema has been modified
  #if multiple schemas in one file, _schema is the last one printed.
  # 2e6ee669 removed _schema, does this still work?
  configure_file(${SCHEMA_FILE} ${SCANNER_BUILD_DIR}/${_schema})
endmacro(SCHEMA_CMLIST SCHEMA_FILE)
