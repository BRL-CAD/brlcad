include(CheckIncludeFile)
check_include_file(ansidecl.h HAVE_ANSIDECL_H)
check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(float.h HAVE_FLOAT_H)
check_include_file(fp_class.h HAVE_FP_CLASS_H)
check_include_file(ieeefp.h HAVE_IEEEFP_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(locale.h HAVE_LOCALE_H)
check_include_file(math.h HAVE_MATH_H)
check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(nan.h HAVE_NAN_H)
check_include_file(stdarg.h HAVE_STDARG_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/timeb.h HAVE_SYS_TIMEB_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(xlocale.h HAVE_XLOCALE_H)

include(CheckFunctionExists)
check_function_exists(asctime HAVE_ASCTIME)
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
check_function_exists(printf HAVE_PRINTF)
check_function_exists(fprintf HAVE_FPRINTF)
check_function_exists(sprintf HAVE_SPRINTF)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(vfprintf HAVE_VFPRINTF)
check_function_exists(vsprintf HAVE_VSPRINTF)
check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(ftime HAVE_FTIME)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(gmtime_r HAVE_GMTIME_R)
check_function_exists(localtime HAVE_LOCALTIME)
check_function_exists(localtime_r HAVE_LOCALTIME_R)
check_function_exists(mktime HAVE_MKTIME)
check_function_exists(sscanf HAVE_SSCANF)
check_function_exists(stat HAVE_STAT)
check_function_exists(time HAVE_TIME)

set(WITH_XSLT_DEBUG 1)
set(WITH_MEM_DEBUG 0)
set(WITH_TRIO 0)
set(WITH_DEBUGGER 1)
set(WITH_MODULES 0)
if(NOT WIN32)
  set(LIBXSLT_DEFAULT_PLUGINS_PATH "${CMAKE_INSTALL_PREFIX}/lib/libxslt-plugins")
endif(NOT WIN32)
if(HAVE_XLOCALE_H)
  set(XSLT_LOCALE_XLOCALE 1)
else(HAVE_XLOCALE_H)
  set(XSLT_LOCALE_XLOCALE 0)
endif(HAVE_XLOCALE_H)
if(WIN32)
  set(XSLT_LOCALE_WINAPI 1)
else(WIN32)
  set(XSLT_LOCALE_WINAPI 0)
endif(WIN32)

set(VERSION "1.1.26")
set(LIBXSLT_VERSION_NUMBER 10126)
set(LIBXSLT_VERSION_EXTRA "-BRLCAD")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libxslt/config.h)
if(COMMAND DISTCLEAN)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../include/libxslt/config.h")
endif(COMMAND DISTCLEAN)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xsltconfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libxslt/xsltconfig.h)
if(COMMAND DISTCLEAN)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../include/libxslt/xsltconfig.h")
endif(COMMAND DISTCLEAN)

set(libxslt_srcs
  src/attributes.c
  src/attrvt.c
  src/documents.c
  src/extensions.c
  src/extra.c
  src/functions.c
  src/imports.c
  src/keys.c
  src/namespaces.c
  src/numbers.c
  src/pattern.c
  src/preproc.c
  src/security.c
  src/templates.c
  src/transform.c
  src/variables.c
  src/xslt.c
  src/xsltlocale.c
  src/xsltutils.c
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/libxslt
  ${CMAKE_CURRENT_SOURCE_DIR}/../libxml/include
  ${CMAKE_CURRENT_BINARY_DIR}/../include
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libxslt
  /usr/local/include
  )

add_library(xslt SHARED ${libxslt_srcs})
target_link_libraries(xslt xml)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
