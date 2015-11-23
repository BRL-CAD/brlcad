cmake_minimum_required(VERSION 2.8)

project (libbson)

include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(TestBigEndian)
include(InstallRequiredSystemLibraries)

set (BSON_MAJOR_VERSION 1)
set (BSON_MINOR_VERSION 3)
set (BSON_MICRO_VERSION 0)
set (BSON_PRERELEASE_VERSION beta0)
set (BSON_API_VERSION 1.0)
set (BSON_VERSION 1.3.0-beta0)

TEST_BIG_ENDIAN(BSON_BIG_ENDIAN)

# These need proper checks like automake.
set (BSON_PTHREAD_ONCE_INIT_NEEDS_BRACES 0)

#librt needed on linux for clock_gettime
#find_library(RT_LIBRARY rt)
#if (RT_LIBRARY)
   #set required libraries for CHECK_FUNCTION_EXISTS
	#set(CMAKE_REQUIRED_LIBRARIES ${RT_LIBRARY})
#endif()

# See https://public.kitware.com/Bug/view.php?id=15659
CHECK_SYMBOL_EXISTS(snprintf stdio.h BSON_HAVE_SNPRINTF)
if (NOT BSON_HAVE_SNPRINTF)
  set(BSON_HAVE_SNPRINTF 0)
else ()
  set(BSON_HAVE_SNPRINTF 1)
endif ()

CHECK_FUNCTION_EXISTS(_set_output_format BSON_NEEDS_SET_OUTPUT_FORMAT)
if (NOT BSON_NEEDS_SET_OUTPUT_FORMAT)
  set(BSON_NEEDS_SET_OUTPUT_FORMAT 0)
endif ()

CHECK_STRUCT_HAS_MEMBER("struct timespec" tv_sec time.h BSON_HAVE_TIMESPEC)
if (NOT BSON_HAVE_TIMESPEC)
  message(STATUS "    no timespec struct")
  set(BSON_HAVE_TIMESPEC 0)
else ()
  message(STATUS "    struct timespec found")
  set(BSON_HAVE_TIMESPEC 1)
endif ()

if (MSVC)
   set (BSON_OS 2)
   set (BSON_HAVE_CLOCK_GETTIME 0)
   set (BSON_HAVE_STDBOOL_H 0)
   set (BSON_HAVE_STRNLEN 0)
   set (BSON_EXTRA_ALIGN 1)
else ()
   find_package (Threads)

   set (BSON_OS 1)
   CHECK_FUNCTION_EXISTS(clock_gettime BSON_HAVE_CLOCK_GETTIME)
   if (NOT BSON_HAVE_CLOCK_GETTIME)
      set(BSON_HAVE_CLOCK_GETTIME 0)
   endif ()
   CHECK_FUNCTION_EXISTS(strnlen BSON_HAVE_STRNLEN)
   if (NOT BSON_HAVE_STRNLEN)
      set(BSON_HAVE_STRNLEN 0)
   endif ()
   CHECK_INCLUDE_FILE(stdbool.h BSON_HAVE_STDBOOL_H)
   if (NOT BSON_HAVE_STDBOOL_H)
      set(BSON_HAVE_STDBOOL_H 0)
   endif ()
   set (BSON_EXTRA_ALIGN 1)
endif ()

set (BSON_HAVE_ATOMIC_32_ADD_AND_FETCH 1)
set (BSON_HAVE_ATOMIC_64_ADD_AND_FETCH 1)

if (BSON_BIG_ENDIAN)
   set (BSON_BYTE_ORDER 4321)
else ()
   set (BSON_BYTE_ORDER 1234)
endif ()

configure_file (
   "src/bson/bson-config.h.in"
   "${CMAKE_CURRENT_BINARY_DIR}/src/bson/bson-config.h"
)

configure_file (
   "src/bson/bson-version.h.in"
   "${CMAKE_CURRENT_BINARY_DIR}/src/bson/bson-version.h"
)

configure_file (
   "${PROJECT_SOURCE_DIR}/build/cmake/bson/bson-stdint.h"
   "${CMAKE_CURRENT_BINARY_DIR}/src/bson/bson-stdint.h"
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/src/bson")
include_directories("src/bson")
include_directories("src")

if (APPLE)
   cmake_policy(SET CMP0042 OLD)
endif()

set (SOURCES
   src/bson/bcon.c
   src/bson/bson.c
   src/bson/bson-atomic.c
   src/bson/bson-clock.c
   src/bson/bson-context.c
   src/bson/bson-error.c
   src/bson/bson-iso8601.c
   src/bson/bson-iter.c
   src/bson/bson-json.c
   src/bson/bson-keys.c
   src/bson/bson-md5.c
   src/bson/bson-memory.c
   src/bson/bson-oid.c
   src/bson/bson-reader.c
   src/bson/bson-string.c
   src/bson/bson-timegm.c
   src/bson/bson-utf8.c
   src/bson/bson-value.c
   src/bson/bson-version-functions.c
   src/bson/bson-writer.c
   src/yajl/yajl_alloc.c
   src/yajl/yajl_buf.c
   src/yajl/yajl.c
   src/yajl/yajl_encode.c
   src/yajl/yajl_gen.c
   src/yajl/yajl_lex.c
   src/yajl/yajl_parser.c
   src/yajl/yajl_tree.c
   src/yajl/yajl_version.c
)

set (HEADERS
   ${CMAKE_CURRENT_BINARY_DIR}/src/bson/bson-config.h
   ${CMAKE_CURRENT_BINARY_DIR}/src/bson/bson-stdint.h
   ${CMAKE_CURRENT_BINARY_DIR}/src/bson/bson-version.h
   src/bson/bcon.h
   src/bson/bson-atomic.h
   src/bson/bson-clock.h
   src/bson/bson-compat.h
   src/bson/bson-context.h
   src/bson/bson-endian.h
   src/bson/bson-error.h
   src/bson/bson.h
   src/bson/bson-iter.h
   src/bson/bson-json.h
   src/bson/bson-keys.h
   src/bson/bson-macros.h
   src/bson/bson-md5.h
   src/bson/bson-memory.h
   src/bson/bson-oid.h
   src/bson/bson-reader.h
   src/bson/bson-stdint-win32.h
   src/bson/bson-string.h
   src/bson/bson-types.h
   src/bson/bson-utf8.h
   src/bson/bson-value.h
   src/bson/bson-version-functions.h
   src/bson/bson-writer.h
)

if (MSVC)
   set(BSON_SHARED_SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/build/cmake/libbson.def)
else()
   set(BSON_SHARED_SOURCES ${SOURCES})
endif()

add_library(bson_shared SHARED ${BSON_SHARED_SOURCES} ${HEADERS})
add_library(bson_static STATIC ${SOURCES} ${HEADERS})

set_target_properties(bson_shared PROPERTIES COMPILE_DEFINITIONS "BSON_COMPILATION")
set_target_properties(bson_static PROPERTIES COMPILE_DEFINITIONS "BSON_COMPILATION")

set_target_properties(bson_shared PROPERTIES VERSION ${BSON_VERSION} SOVERSION ${BSON_MAJOR_VERSION})
set_target_properties(bson_static PROPERTIES VERSION ${BSON_VERSION} SOVERSION ${BSON_MAJOR_VERSION})

set_target_properties(bson_shared PROPERTIES OUTPUT_NAME "bson-${BSON_API_VERSION}" PREFIX "lib")
set_target_properties(bson_static PROPERTIES OUTPUT_NAME "bson-static-${BSON_API_VERSION}")

if (RT_LIBRARY)
    target_link_libraries (bson_shared ${RT_LIBRARY})
    target_link_libraries (bson_static ${RT_LIBRARY})
endif()

if (UNIX)
    target_link_libraries (bson_shared ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries (bson_static ${CMAKE_THREAD_LIBS_INIT})
else()
    target_link_libraries (bson_shared ws2_32)
    target_link_libraries (bson_static ws2_32)
endif()

enable_testing()

add_executable(test-libbson
   tests/TestSuite.c
   tests/TestSuite.h
   tests/test-libbson.c
   tests/test-atomic.c
   tests/test-bson.c
   tests/test-endian.c
   tests/test-clock.c
   tests/test-error.c
   tests/test-iso8601.c
   tests/test-iter.c
   tests/test-json.c
   tests/test-oid.c
   tests/test-reader.c
   tests/test-string.c
   tests/test-utf8.c
   tests/test-value.c
   tests/test-version.c
   tests/test-writer.c
   tests/test-bcon-basic.c
   tests/test-bcon-extract.c
)

target_link_libraries(test-libbson bson_static)
add_test(NAME test-libbson COMMAND test-libbson)

file(COPY tests/binary tests/json
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)

install(
  TARGETS bson_shared bson_static
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)
install(
  FILES ${HEADERS}
  DESTINATION "include/libbson-${BSON_API_VERSION}"
)

set(VERSION "${BSON_VERSION}")
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(libdir "\${prefix}/lib")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libbson-1.0.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libbson-1.0.pc
@ONLY)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libbson-1.0.pc
    DESTINATION lib/pkgconfig
)
