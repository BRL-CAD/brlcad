find_package(Threads)
include(CheckIncludeFile)
if(Threads_FOUND)
  check_include_file(pthread.h HAVE_PTHREAD_H)
  add_definitions(-DLIBXML_THREAD_ENABLED)
endif(Threads_FOUND)
if(CMAKE_USE_PTHREADS_INIT)
  set(HAVE_LIBPTHREAD 1)
endif(CMAKE_USE_PTHREADS_INIT)

check_include_file(ansidecl.h HAVE_ANSIDECL_H)
check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(arpa/nameser.h HAVE_ARPA_NAMESER_H)
check_include_file(ctype.h HAVE_CTYPE_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(dl.h HAVE_DL_H)
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(float.h HAVE_FLOAT_H)
check_include_file(fp_class.h HAVE_FP_CLASS_H)
check_include_file(ieeefp.h HAVE_IEEEFP_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(inttypes.h.h HAVE_INTTYPES_H_H)
check_include_file(limits.h HAVE_LIMITS_H)
check_include_file(malloc.h HAVE_MALLOC_H)
check_include_file(math.h HAVE_MATH_H)
check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(nan.h HAVE_NAN_H)
check_include_file(ndir.h HAVE_NDIR_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(netinet/in.h HAVE_NETINET_IN_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(resolv.h HAVE_RESOLV_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(stdarg.h HAVE_STDARG_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(string.h HAVE_STRING_H)
#check_include_file(sys/dir.h HAVE_SYS_DIR_H)
check_include_file(sys/mman.h HAVE_SYS_MMAN_H)
check_include_file(sys/ndir.h HAVE_SYS_NDIR_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/timeb.h HAVE_SYS_TIMEB_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)

include(CheckFunctionExists)
check_function_exists(finite HAVE_FINITE)
check_function_exists(printf HAVE_PRINTF)
check_function_exists(fprintf HAVE_FPRINTF)
check_function_exists(sprintf HAVE_SPRINTF)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(vfprintf HAVE_VFPRINTF)
check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(vsprintf HAVE_VSPRINTF)
check_function_exists(ftime HAVE_FTIME)
check_function_exists(getaddrinfo HAVE_GETADDRINFO)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(isinf HAVE_ISINF)
check_function_exists(isnan HAVE_ISNAN)
check_function_exists(localtime HAVE_LOCALTIME)
check_function_exists(signal HAVE_SIGNAL)
check_function_exists(sscanf HAVE_SSCANF)
if(NOT MSVC)
  check_function_exists(stat HAVE_STAT)
else(NOT MSVC)
  set(HAVE_STAT 0)
endif(NOT MSVC)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strndup HAVE_STRNDUP)
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strftime HAVE_STRFTIME)

include(CheckCSourceCompiles)
set(va_copy_srcs "
#include <stdarg.h> 
va_list ap1,ap2; 
int main () { 
va_copy(ap1,ap2); 
return 0; 
}
")
check_c_source_compiles("${va_copy_srcs}" HAVE_VA_COPY)

check_function_exists(__va_copy HAVE___VA_COPY)

include(CheckLibraryExists)
check_library_exists(dl dlopen "" HAVE_STD_DLOPEN)
if(HAVE_STD_DLOPEN)
  set(DL_LIBRARY "dl")
  set(HAVE_DLOPEN 1)
else(HAVE_STD_DLOPEN)
  check_library_exists(c dlopen "" HAVE_C_DLOPEN)
  if(HAVE_C_DLOPEN)
    set(DL_LIBRARY "c")
    set(HAVE_DLOPEN 1)
  endif(HAVE_C_DLOPEN)
endif(HAVE_STD_DLOPEN)


check_library_exists(m pow "" HAVE_MATH_LIB)
if(HAVE_MATH_LIB)
  set(M_LIBRARY "m")
endif(HAVE_MATH_LIB)

set(WITH_TRIO 0)
if(CMAKE_THREAD_LIBS_INIT OR MSVC)
  set(WITH_THREADS 1)
else(CMAKE_THREAD_LIBS_INIT OR MSVC)
  set(WITH_THREADS 0)
endif(CMAKE_THREAD_LIBS_INIT OR MSVC)
set(WITH_TREE 1)
set(WITH_OUTPUT 1)
set(WITH_PUSH 1)
set(WITH_READER 1)
set(WITH_PATTERN 1)
set(WITH_WRITER 1)
set(WITH_SAX1 1)
set(WITH_FTP 0)
set(WITH_HTTP 0)
set(WITH_VALID 1)
set(WITH_HTML 1)
set(WITH_LEGACY 1)
set(WITH_C14N 1)
set(WITH_CATALOG 1)
set(WITH_DOCB 1)
set(WITH_XPATH 1)
set(WITH_XPTR 1)
set(WITH_XINCLUDE 1)
set(WITH_ICONV 0)
set(WITH_ICU 0)
set(WITH_ISO8859X 1)
set(WITH_DEBUG 1)
set(WITH_MEM_DEBUG 0)
set(WITH_RUN_DEBUG 0)
set(WITH_REGEXPS 1)
set(WITH_SCHEMAS 1)
set(WITH_SCHEMATRON 1)
set(WITH_MODULES 0)
set(WITH_ZLIB 0)

set(LIBXML_VERSION "2.7.8")
set(LIBXML_VERSION_NUMBER "20708")
set(LIBXML_VERSION_EXTRA "-BRLCAD")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/libxml/xmlversion.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libxml/xmlversion.h)

if(COMMAND DISTCLEAN)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../include/libxml/xmlversion.h")
endif(COMMAND DISTCLEAN)

if(WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/libxml/xmlwin32version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/../include/libxml/xmlwin32version.h)
  if(COMMAND DISTCLEAN)
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../include/libxml/xmlwin32version.h")
  endif(COMMAND DISTCLEAN)
endif(WIN32)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libxml/config.h)
if(COMMAND DISTCLEAN)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../include/libxml/config.h")
endif(COMMAND DISTCLEAN)

add_definitions(-DHAVE_CONFIG_H)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/../include
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libxml/
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  /usr/local/include
  )

if(WITH_TRIO)
  set(trio_srcs
    src/trio.c
    src/triostr.c
    )
else(WITH_TRIO)
  set(trio_srcs
    src/trionan.c
    )
endif(WITH_TRIO)

set(libxml_srcs
  src/DOCBparser.c
  src/HTMLparser.c
  src/HTMLtree.c
  src/SAX.c
  src/SAX2.c
  src/c14n.c
  src/catalog.c
  src/chvalid.c
  src/debugXML.c
  src/dict.c
  src/encoding.c
  src/entities.c
  src/error.c
  src/globals.c
  src/hash.c
  src/list.c
  src/parser.c
  src/parserInternals.c
  src/pattern.c
  src/relaxng.c
  src/schematron.c
  src/threads.c
  src/tree.c
  ${trio_srcs}
  src/uri.c
  src/valid.c
  src/xinclude.c
  src/xlink.c
  src/xlink.c
  src/xmlIO.c
  src/xmlmemory.c
  src/xmlmodule.c
  src/xmlreader.c
  src/xmlregexp.c
  src/xmlsave.c
  src/xmlschemas.c
  src/xmlschemastypes.c
  src/xmlstring.c
  src/xmlwriter.c
  src/xmlunicode.c
  src/xpath.c
  src/xpointer.c
  )

add_library(xml SHARED ${libxml_srcs})
target_link_libraries(xml ${DL_LIBRARY} Threads::Threads ${M_LIBRARY})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
