# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(XSLTPROC)

INCLUDE(CheckIncludeFile)
CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE(time.h HAVE_TIME_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)

ADD_SUBDIRECTORY(libxml)
ADD_SUBDIRECTORY(libxslt)
ADD_SUBDIRECTORY(libexslt)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/libxml/include
	${CMAKE_CURRENT_SOURCE_DIR}/libxslt/include
	${CMAKE_CURRENT_SOURCE_DIR}/libxslt/include/libxslt
	${CMAKE_CURRENT_SOURCE_DIR}/libexslt/include
	${CMAKE_CURRENT_BINARY_DIR}/include
	${CMAKE_CURRENT_BINARY_DIR}/include/libxslt
	)

add_executable(xsltproc xsltproc.c)
target_link_libraries(xsltproc xml xslt exslt)
