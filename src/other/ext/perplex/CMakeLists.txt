cmake_minimum_required(VERSION 3.12)

# set project name
project(CITRUS)

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT BIN_DIR)
  set (BIN_DIR bin)
endif (NOT BIN_DIR)
if (NOT DATA_DIR)
  set (DATA_DIR share)
endif (NOT DATA_DIR)

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

configure_file(re2c/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
add_definitions(-DHAVE_CONFIG_H)

# build the lemon binary
add_executable(lemon lemon.c)


set(re2c_common_SRCS
  re2c/code.cc
  re2c/dfa.cc
  re2c/main.cc
  re2c/actions.cc
  re2c/substr.cc
  re2c/translate.cc
  re2c/mbo_getopt.cc
  )

set(re2c_HDRS
  re2c/basics.h
  re2c/dfa.h
  re2c/globals.h
  re2c/ins.h
  re2c/re2c_parser.h
  re2c/re.h
  re2c/scanner.h
  re2c/substr.h
  re2c/token.h
  re2c/mbo_getopt.h
  re2c/code.h
  re2c/stream_lc.h
  re2c/code_names.h
  )

set(WD "${CMAKE_CURRENT_BINARY_DIR}/bootstrap_parser")
file(MAKE_DIRECTORY "${WD}")
add_custom_command(
  OUTPUT ${WD}/parser.out ${WD}/parser.c ${WD}/parser.h
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/re2c/parser.yy ${WD}/parser.yy
  COMMAND lemon -T${CMAKE_CURRENT_SOURCE_DIR}/lempar.c ${WD}/parser.yy
  DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/re2c/parser.yy
  WORKING_DIRECTORY ${WD}
  COMMENT "[LEMON][re2c] Building bootstrap parser with lemon"
  )

add_custom_command(
  OUTPUT bootstrap_parser.cc bootstrap_parser_tokens.h
  COMMAND ${CMAKE_COMMAND} -E copy ${WD}/parser.c bootstrap_parser.cc
  COMMAND ${CMAKE_COMMAND} -E copy ${WD}/parser.h bootstrap_parser_tokens.h
  DEPENDS ${WD}/parser.c ${WD}/parser.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

set(re2c_bootstrap_SRCS
  ${re2c_common_SRCS}
  re2c/bootstrap/scanner.cc
  ${CMAKE_CURRENT_BINARY_DIR}/bootstrap_parser.cc
  ${CMAKE_CURRENT_BINARY_DIR}/bootstrap_parser_tokens.h
  )
add_executable(re2c_bootstrap ${re2c_bootstrap_SRCS})
set_property(TARGET re2c_bootstrap APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/re2c;${CMAKE_CURRENT_BINARY_DIR};${WD}")
add_dependencies(re2c_bootstrap lemon)
# Need to link against stl library with some compilers.
target_link_libraries(re2c_bootstrap "${STDCXX_LIBRARIES}")

if(TARGET_RUNTIME_OUTPUT_DIRECTORY)
  set_target_properties(re2c_bootstrap PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TARGET_RUNTIME_OUTPUT_DIRECTORY}")
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    set_target_properties(re2c_bootstrap PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER} "${TARGET_RUNTIME_OUTPUT_DIRECTORY}")
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
endif(TARGET_RUNTIME_OUTPUT_DIRECTORY)

set(WD "${CMAKE_CURRENT_BINARY_DIR}/re2c_parser")
file(MAKE_DIRECTORY "${WD}")
add_custom_command(
  OUTPUT ${WD}/parser.out ${WD}/parser.c ${WD}/parser.h
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/re2c/parser.yy ${WD}/parser.yy
  COMMAND lemon -T${CMAKE_CURRENT_SOURCE_DIR}/lempar.c ${WD}/parser.yy
  DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/re2c/parser.yy
  WORKING_DIRECTORY ${WD}
  COMMENT "[LEMON][re2c] Building parser with lemon"
  )

add_custom_command(
  OUTPUT re2c_parser.cc re2c_parser_tokens.h
  COMMAND ${CMAKE_COMMAND} -E copy ${WD}/parser.c re2c_parser.cc
  COMMAND ${CMAKE_COMMAND} -E copy ${WD}/parser.h re2c_parser_tokens.h
  DEPENDS ${WD}/parser.c ${WD}/parser.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

add_custom_command(
  OUTPUT re2c_scanner.cc
  COMMAND re2c_bootstrap -bi -o re2c_scanner.cc ${CMAKE_CURRENT_SOURCE_DIR}/re2c/scanner.re
  DEPENDS re2c_bootstrap ${CMAKE_CURRENT_SOURCE_DIR}/re2c/scanner.re
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

set(re2c_SRCS
  ${re2c_common_SRCS}
  ${CMAKE_CURRENT_BINARY_DIR}/re2c_parser.cc
  ${CMAKE_CURRENT_BINARY_DIR}/re2c_parser_tokens.h
  ${CMAKE_CURRENT_BINARY_DIR}/re2c_scanner.cc
  )

add_executable(re2c ${re2c_SRCS})
set_property(TARGET re2c APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/re2c;${CMAKE_CURRENT_BINARY_DIR};${WD}")
# Need to link against stl library with some compilers.
target_link_libraries(re2c "${STDCXX_LIBRARIES}")



set(WD "${CMAKE_CURRENT_BINARY_DIR}/perplex_wd")
file(MAKE_DIRECTORY "${WD}")
add_custom_command(
  OUTPUT ${WD}/parser.out ${WD}/parser.c ${WD}/parser.h
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/perplex/parser.y ${WD}/parser.y
  COMMAND lemon -T${CMAKE_CURRENT_SOURCE_DIR}/lempar.c ${WD}/parser.y
  DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/perplex/parser.y
  WORKING_DIRECTORY ${WD}
  COMMENT "[LEMON][perplex] Building parser with lemon"
  )
add_custom_command(
  OUTPUT perplex_parser.c perplex_parser.h
  COMMAND ${CMAKE_COMMAND} -E copy ${WD}/parser.c perplex_parser.c
  COMMAND ${CMAKE_COMMAND} -E copy ${WD}/parser.h perplex_parser.h
  DEPENDS ${WD}/parser.c ${WD}/parser.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/perplex_scanner.c
  COMMAND re2c -c -o ${CMAKE_CURRENT_BINARY_DIR}/perplex_scanner.c ${CMAKE_CURRENT_SOURCE_DIR}/perplex/scanner.re
  DEPENDS re2c ${CMAKE_CURRENT_SOURCE_DIR}/perplex/scanner.re perplex_parser.c perplex_parser.h
  WORKING_DIRECTORY ${WD}
  COMMENT "[RE2C][perplex] Building scanner with re2c"
  )

set(perplex_sources
  ${CMAKE_CURRENT_BINARY_DIR}/perplex_parser.c
  ${CMAKE_CURRENT_BINARY_DIR}/perplex_scanner.c
  perplex/mbo_getopt.cpp
  perplex/perplex.cpp
  )

add_executable(perplex ${perplex_sources})
set_property(TARGET perplex APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/perplex")



# install the binaries
install(TARGETS lemon DESTINATION ${BIN_DIR})
install(TARGETS re2c DESTINATION ${BIN_DIR})
install(TARGETS perplex DESTINATION ${BIN_DIR})

# install the templates
install(FILES lempar.c DESTINATION ${DATA_DIR}/lemon)
install(FILES perplex/perplex_template.c DESTINATION ${DATA_DIR}/perplex)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
