# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
endif(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(RE2C)

# Set project information
set(PACKAGE re2c)
set(VERSION 0.13.5)
set(PACKAGE_NAME ${PACKAGE})
set(PACKAGE_BUGREPORT "re2c-general@lists.sourceforge.net")
set(PACKAGE_STRING "${PACKAGE} ${VERSION}")
set(PACKAGE_TARNAME ${PACKAGE})
set(PACKAGE_VERSION ${VERSION})
set(PACKAGE_RELEASE 1)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/CMake")
find_package(LEMON)
if(NOT COMMAND LEMON_TARGET)
  include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/LEMON_Util.cmake)
endif(NOT COMMAND LEMON_TARGET)

LEMON_TARGET(re2cParse parser.yy parser.cc parser_tokens.h)

set(re2c_common_SRCS
  code.cc
  dfa.cc
  main.cc
  ${LEMON_re2cParse_OUTPUT_SOURCE}
  actions.cc
  substr.cc
  translate.cc
  mbo_getopt.cc
  )

set(re2c_HDRS
  basics.h
  dfa.h
  globals.h
  ins.h
  re2c_parser.h
  re.h
  scanner.h
  substr.h
  token.h
  mbo_getopt.h
  code.h
  stream_lc.h
  code_names.h
  )

set(STDC_HEADERS 1)
include(CheckIncludeFile)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILE(stdbool.h HAVE_STDBOOL_H)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(memset HAVE_MEMSET)
CHECK_FUNCTION_EXISTS(mmap HAVE_MMAP)
CHECK_FUNCTION_EXISTS(munmap HAVE_MUNMAP)
CHECK_FUNCTION_EXISTS(strdup HAVE_STRDUP)
CHECK_FUNCTION_EXISTS(strndup HAVE_STRNDUP)
include(CheckTypeSize)
CHECK_TYPE_SIZE(char SIZEOF_CHAR)
CHECK_TYPE_SIZE(int SIZEOF_INT)
CHECK_TYPE_SIZE(long SIZEOF_LONG)
CHECK_TYPE_SIZE(short SIZEOF_SHORT)

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
add_definitions(-DHAVE_CONFIG_H)


set(re2c_bootstrap_SRCS
  ${re2c_common_SRCS}
  bootstrap/scanner.cc
  )
set_source_files_properties(bootstrap/scanner.cc PROPERTIES OBJECT_DEPENDS ${LEMON_re2cParse_OUTPUT_HEADER})

add_executable(re2c_bootstrap ${re2c_bootstrap_SRCS})

# Need to link against stl library with some compilers.
target_link_libraries(re2c_bootstrap "${STDCXX_LIBRARIES}")

if(TARGET_RUNTIME_OUTPUT_DIRECTORY)
  set_target_properties(re2c_bootstrap PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TARGET_RUNTIME_OUTPUT_DIRECTORY}")
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    set_target_properties(re2c_bootstrap PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER} "${TARGET_RUNTIME_OUTPUT_DIRECTORY}")
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
endif(TARGET_RUNTIME_OUTPUT_DIRECTORY)
if(${LEMON_EXECUTABLE_TARGET})
  add_dependencies(re2c_bootstrap ${LEMON_EXECUTABLE_TARGET})
endif(${LEMON_EXECUTABLE_TARGET})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/scanner.cc
  COMMAND re2c_bootstrap -bi -o ${CMAKE_CURRENT_BINARY_DIR}/scanner.cc	${CMAKE_CURRENT_SOURCE_DIR}/scanner.re
  DEPENDS re2c_bootstrap 
  )

set(re2c_SRCS
  ${re2c_common_SRCS}
  ${CMAKE_CURRENT_BINARY_DIR}/scanner.cc
  )

add_executable(re2c ${re2c_SRCS})

# Need to link against stl library with some compilers.
target_link_libraries(re2c "${STDCXX_LIBRARIES}")

# Install
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT RE2C_SKIP_INSTALL)
  install(TARGETS re2c DESTINATION ${BIN_DIR})
endif(NOT RE2C_SKIP_INSTALL)

configure_file(README.in ${CMAKE_CURRENT_BINARY_DIR}/README)
configure_file(run_tests.sh.in ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh @ONLY)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
