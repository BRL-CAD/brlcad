# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(M4)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
find_package(REGEX)

SET(M4_SRCS
	eval.c
	expr.c
	gnum4.c
	look.c
	main.c
	misc.c
	trace.c
	lib/ohash_create_entry.c
	lib/ohash_delete.c
	lib/ohash_do.c
	lib/ohash_entries.c
	lib/ohash_enum.c
	lib/ohash.h
	lib/ohash_init.3
	lib/ohash_init.c
	lib/ohash_interval.3
	lib/ohash_interval.c
	lib/ohash_int.h
	lib/ohash_lookup_interval.c
	lib/ohash_lookup_memory.c
	lib/ohash_qlookup.c
	lib/ohash_qlookupi.c
	lib/strtonum.c
	lib/strlcpy.c
	lib/getprogname.c
	lib/setprogname.c
	lib/libyywrap.c
	generated/parser.c
	generated/tokenizer.c
)

INCLUDE_DIRECTORIES(
	${REGEX_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/lib
	)

add_executable(m4 ${M4_SRCS})
target_link_libraries(m4 ${REGEX_LIBRARY})
SET_TARGET_PROPERTIES(m4 PROPERTIES COMPILE_FLAGS "-DEXTENDED")
