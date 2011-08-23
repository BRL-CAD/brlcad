# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(RE2C)

# Set project information
SET(PACKAGE re2c)
SET(VERSION 0.13.5)
SET(PACKAGE_NAME ${PACKAGE})
SET(PACKAGE_BUGREPORT "re2c-general@lists.sourceforge.net")
SET(PACKAGE_STRING "${PACKAGE} ${VERSION}")
SET(PACKAGE_TARNAME ${PACKAGE})
SET(PACKAGE_VERSION ${VERSION})

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
find_package(YACC)

IF(YACC_EXECUTABLE)
	YACC_TARGET(re2cParser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cc COMPILE_FLAGS "-d")
	SET(YACC_PARSER_SRCS ${YACC_re2cParser_OUTPUTS})
ELSE(YACC_EXECUTABLE)
	SET(YACC_PARSER_SRCS bootstrap/parser.cc)
	INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/bootstrap)
ENDIF(YACC_EXECUTABLE)

SET(re2c_common_SRCS
	code.cc 
	dfa.cc 
	main.cc 
	${YACC_PARSER_SRCS}
	actions.cc 
	substr.cc
	translate.cc 
	mbo_getopt.cc
	)

SET(re2c_HDRS
	basics.h 
	dfa.h 
	globals.h 
	ins.h 
	parser.h 
	re.h 
	scanner.h
	substr.h
	token.h 
	mbo_getopt.h 
	code.h 
	stream_lc.h 
	code_names.h
	)

SET(STDC_HEADERS 1)
INCLUDE(CheckIncludeFile)
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
INCLUDE(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(memset HAVE_MEMSET)
CHECK_FUNCTION_EXISTS(mmap HAVE_MMAP)
CHECK_FUNCTION_EXISTS(munmap HAVE_MUNMAP)
CHECK_FUNCTION_EXISTS(strdup HAVE_STRDUP)
CHECK_FUNCTION_EXISTS(strndup HAVE_STRNDUP)
INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE(char SIZEOF_CHAR)
CHECK_TYPE_SIZE(int SIZEOF_INT)
CHECK_TYPE_SIZE(long SIZEOF_LONG)
CHECK_TYPE_SIZE(short SIZEOF_SHORT)

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
add_definitions(-DHAVE_CONFIG_H)

SET(re2c_bootstrap_SRCS
	${re2c_common_SRCS}
	bootstrap/scanner.cc
	)

add_executable(re2c_bootstrap ${re2c_bootstrap_SRCS})

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/scanner.cc
	COMMAND re2c_bootstrap -bi -o ${CMAKE_CURRENT_BINARY_DIR}/scanner.cc	${CMAKE_CURRENT_SOURCE_DIR}/scanner.re
	DEPENDS re2c_bootstrap
	)

SET(re2c_SRCS
	${re2c_common_SRCS}
	${CMAKE_CURRENT_BINARY_DIR}/scanner.cc
	)

add_executable(re2c ${re2c_SRCS})

configure_file(README.in ${CMAKE_CURRENT_BINARY_DIR}/README)
configure_file(run_tests.sh.in ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh @ONLY)

	#	man_MANS     = re2c.1

#CXXFLAGS     = -O2 -Wall -Wno-unused -Wno-parentheses -Wno-deprecated
#CXXFLAGS     = -ggdb -fno-inline -O2 -Wall -Wextra -pedantic -Wconversion -Wpointer-arith -Wwrite-strings -Wredundant-decls -Werror -Wunused-function -DPEDANTIC

#RE2CFLAGS    = -bi

#CLEANFILES   = parser.cc y.tab.c y.tab.h scanner.cc re2c.1 .version htdocs/manual.html


