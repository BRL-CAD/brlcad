#                     C M A K E L I S T S . T X T
# CMake build for GDAL and PROJ4 - the following copyright/license
# applies only to the CMake build logic.  See GDAL/PROJ4 documentation
# for their licenses.
#
# Copyright (c) 2017 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(GDAL)

if(NOT MSVC OR "${MSVC_VERSION}" GREATER 1700)
   set(CMAKE_CXX_STANDARD 11)
endif(NOT MSVC OR "${MSVC_VERSION}" GREATER 1700)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

find_package(PROJ4 REQUIRED)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/port
  ${CMAKE_CURRENT_SOURCE_DIR}/gcore
  ${CMAKE_CURRENT_SOURCE_DIR}/alg
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts
  ${CMAKE_CURRENT_SOURCE_DIR}/gnm
  ${CMAKE_CURRENT_SOURCE_DIR}/apps
  ${PROJ4_INCLUDE_DIR}
  )

if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT DATA_DIR)
  set(DATA_DIR share)
endif(NOT DATA_DIR)

if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)

if (NOT DEFINED BUILD_STATIC_LIBS)
  set(BUILD_STATIC_LIBS OFF)
endif (NOT DEFINED BUILD_STATIC_LIBS)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

find_package(SSE) # checks SSE, AVX and AVX2

CHECK_SSE(CXX "AVX" " ;-mavx;/arch:AVX")
IF(C_AVX_FOUND)
  MESSAGE(STATUS "AVX Found")
  add_definitions(-DHAVE_AVX_AT_COMPILE_TIME)
ENDIF(C_AVX_FOUND)

CHECK_SSE(CXX "SSE" " ;-msse;/arch:SSE")
IF(C_SSE_FOUND)
  MESSAGE(STATUS "SSE Found")
  add_definitions(-DHAVE_SSE_AT_COMPILE_TIME)
ENDIF(C_SSE_FOUND)

CHECK_SSE(CXX "SSSE3" " ;-mssse3;/arch:SSE3")
IF(C_SSSE3_FOUND)
  MESSAGE(STATUS "MSSSE3 Found")
  add_definitions(-DHAVE_SSSE3_AT_COMPILE_TIME)
ENDIF(C_SSSE3_FOUND)

add_definitions(-DPROJ_STATIC)
add_definitions(-DGDAL_COMPILATION)
add_definitions(-DGNM_ENABLED)
add_definitions(-DGNMFILE_ENABLED)
add_definitions(-DGNMDB_ENABLED)
add_definitions(-DUSE_ONLY_CRYPTODLL_ALG)
if (NOT DEFINED GDAL_INST_DATA_DIR)
  add_definitions(-DINST_DATA=\"${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/gdal\")
else (NOT DEFINED GDAL_INST_DATA_DIR)
  add_definitions(-DINST_DATA=\"${GDAL_INST_DATA_DIR}/gdal\")
endif (NOT DEFINED GDAL_INST_DATA_DIR)
set(HAVE_IEEEFP 1) # TODO test this for real

# see if we have nullptr: https://stackoverflow.com/a/14112019
set(nullptr_src "
#include <cstddef>
#include <iostream>
int retval;
void r(int i) {if (i < 0) {std::cout << \"Error - wrong function called\";} retval = 1;}
void r(char *c){retval = (!c) ? 0 : 1;}
int main() {
  retval = 1;
  r(nullptr);
  return retval;
}")
include(CheckCXXSourceRuns)
check_cxx_source_runs("${nullptr_src}" HAVE_NULLPTR)

if(HAVE_NULLPTR)
  if(NOT MSVC OR "${MSVC_VERSION}" GREATER 1700)
     add_definitions(-DHAVE_CXX11)
  endif(NOT MSVC OR "${MSVC_VERSION}" GREATER 1700)
  add_definitions(-DNULL_AS_NULLPTR)
endif(HAVE_NULLPTR)

if(MSVC)
  add_definitions(-DUSE_WIN32_MUTEX)
  add_definitions(-DNOMINMAX)
endif(MSVC)

find_package(ZLIB)
if(ZLIB_LIBRARIES)
  include_directories(${ZLIB_INCLUDE_DIRS})
  add_definitions(-DHAVE_LIBZ)
endif(ZLIB_LIBRARIES)
find_package(PNG)
if(PNG_LIBRARIES)
  include_directories(${PNG_INCLUDE_DIRS})
  add_definitions(-DUSE_PNG)
endif(PNG_LIBRARIES)


include(CheckTypeSize)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size("unsigned long" SIZEOF_ULONG)
set(SIZEOF_UNSIGNED_LONG ${SIZEOF_ULONG})
set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})
check_type_size("long long" HAVE_LONG_LONG)
check_type_size(int8 HAVE_INT8)
check_type_size(int16 HAVE_INT16)
check_type_size(int32 HAVE_INT32)
check_type_size(__uint128_t HAVE_UINT128_T)
check_type_size(uintptr_t HAVE_UINTPTR_T)

include(CheckIncludeFiles)
check_include_files(assert.h HAVE_ASSERT_H)
check_include_files(csf.h HAVE_CSF_H)
check_include_files(dbmalloc.h HAVE_DBMALLOC_H)
check_include_files(dirent.h HAVE_DIRENT_H)
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(errno.h HAVE_ERRNO_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(float.h HAVE_FLOAT_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(limits.h HAVE_LIMITS_H)
check_include_files(locale.h HAVE_LOCALE_H)
check_include_files(memory.h HAVE_MEMORY_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(values.h HAVE_VALUES_H)
check_include_files(xlocale.h HAVE_XLOCALE_H)

include(CheckFunctionExists)
check_function_exists(localeconv HAVE_LOCALECONV)
check_function_exists(atoll HAVE_ATOLL)
check_function_exists(getcwd HAVE_GETCWD)
check_function_exists(iconv HAVE_ICONV)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(strtof HAVE_STRTOF)
check_function_exists(vprintf HAVE_VPRINTF)
check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(readlink HAVE_READLINK)
check_function_exists(posix_spawnp HAVE_POSIX_SPAWNP)
check_function_exists(posix_memalign HAVE_POSIX_MENALIGN)
check_function_exists(vfork HAVE_VFORK)
check_function_exists(mmap HAVE_MMAP)
check_function_exists(statvfs HAVE_STATVFS)
check_function_exists(statvfs64 HAVE_STATVFS64)
check_function_exists(lstat HAVE_LSTAT)
check_function_exists(uselocale HAVE_USELOCALE)

include(CheckLibraryExists)
check_library_exists(m ceil "" HAVE_LIBM)
if(HAVE_LIBM)
  set(M_LIBRARY m)
endif(HAVE_LIBM)
check_library_exists(dl dlopen "" HAVE_LIBDL)
if(HAVE_LIBDL)
  set(DL_LIBRARY dl)
endif(HAVE_LIBDL)

set(HOST_FILLORDER "FILLORDER_LSB2MSB")
if(NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" AND NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "i*86" AND NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64")
  set(HOST_FILLORDER "FILLORDER_MSB2LSB")
endif(NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" AND NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "i*86" AND NOT "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64")

# Threads
CHECK_INCLUDE_FILE(pthread.h PROBE_PTHREAD_H)
if (NOT PROBE_PTHREAD_H)
  cmake_push_check_state(RESET)
  # pthread.h on FreeBSD 10 and some older Linucies use non-c90 types
  set(CMAKE_REQUIRED_DEFINITIONS "-Dclockid_t=clock_t")
  set(CMAKE_REQUIRED_FLAGS "-pthread")
  CHECK_INCLUDE_FILE(pthread.h PROBE_PTHREAD_H_CLOCKID_T)
  if (PROBE_PTHREAD_H_CLOCKID_T)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dclockid_t=clock_t -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dclockid_t=clock_t -pthread")
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  endif (PROBE_PTHREAD_H_CLOCKID_T)
endif (NOT PROBE_PTHREAD_H)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)
if(CMAKE_USE_PTHREADS_INIT)
  set(CPL_MULTIPROC_PTHREAD 1)
  include(CheckSymbolExists)
  check_symbol_exists(PTHREAD_MUTEX_RECURSIVE pthread.h HAVE_PTHREAD_MUTEX_RECURSIVE)
  check_symbol_exists(PTHREAD_MUTEX_ADAPTIVE_NP pthread.h HAVE_PTHREAD_MUTEX_ADAPTIVE_NP)
  check_type_size(pthread_spinlock_t HAVE_PTHREAD_SPINLOCK)
endif(CMAKE_USE_PTHREADS_INIT)

# Need iconv library if it's around
find_library(ICONV_LIBRARY NAMES iconv)
mark_as_advanced(ICONV_LIBRARY)

# Clang usually matches GNU, but linker flags are different on Apple
# if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
#   set(fail_on_undefined_flag "Wl,--no-undefined")
# elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
#   set(fail_on_undefined_flag "Wl,-undefined,error")
# endif()

# Make sure to put these after the other tests, so we don't
# mess up the detections with Wl,--no-undefined
set(cmp_flags "Wno-error" g O2 fPIC DPIC "${fail_on_undefined_flag}" W4 wd4127 wd4251 wd4275 wd4786 wd4100 wd4245 wd4206 wd4351 wd4611)
foreach(cf ${cmp_flags})
  check_c_compiler_flag(-${cf} ${cf}_FLAG_C)
  if(${cf}_FLAG_C)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -${cf}")
  endif(${cf}_FLAG_C)
  check_cxx_compiler_flag(-${cf} ${cf}_FLAG_CXX)
  if(${cf}_FLAG_CXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -${cf}")
  endif(${cf}_FLAG_CXX)
endforeach(cf ${cmp_flags})

# Core essential formats
set(GDAL_FORMATS raw vrt mem gtiff derived hfa)
set(OGR_FORMATS generic vrt mem mitab geojson xplane)

# Additional formats
set(GDAL_FORMATS
  ${GDAL_FORMATS}
  aaigrid adrg aigrid airsar arg
  blx bmp
  cals ceos ceos2 coasp cosar ctg
  dimap dted
  e00grid elas envisat ers
  fit
  gff gsg gxf
  hf2
  idrisi ilwis ingr iris iso8211
  jaxapalsar jdem
  kmlsuperoverlay
  l1b leveller
  map
  ngsgeoid nitf northwood
  ozi
  pds png prf
  r rik rmf rs2
  safe saga
  sentinel2 sgi srtmhgt
  terragen til tsx
  usgsdem
  xpm xyz
  zmap
  )

set(OGR_FORMATS
  ${OGR_FORMATS}
  aeronavfaa arcgen avc
  bna
  csv
  dgn dxf
  edigeo
  geoconcept gmt gpsbabel gtm
  htf
  idrisi
  ntf
  openair openfilegdb
  pds pgdump
  rec
  s57 segukooa segy selafin shape sua sxf
  tiger
  vdv
  wasp
  )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h.tmp)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h.tmp)

add_subdirectory(port)
add_subdirectory(gcore)
add_subdirectory(alg)
add_subdirectory(frmts)
add_subdirectory(ogr)
add_subdirectory(gnm)
add_subdirectory(apps)

set(gdal_srcs
  $<TARGET_OBJECTS:gdal_cpl_obj>
  $<TARGET_OBJECTS:gdal_mdreader_obj>
  $<TARGET_OBJECTS:gdal_gcore_obj>
  $<TARGET_OBJECTS:gdal_alg_obj>
  $<TARGET_OBJECTS:gdal_allreg_obj>
  $<TARGET_OBJECTS:libtiff_obj>
  $<TARGET_OBJECTS:libgeotiff_obj>
  $<TARGET_OBJECTS:json_obj>
  $<TARGET_OBJECTS:gdal_ogr_obj>
  $<TARGET_OBJECTS:gdal_gnm_obj>
  $<TARGET_OBJECTS:gdal_apps_obj>
  )
foreach(frmt ${GDAL_FORMATS})
  set(gdal_srcs ${gdal_srcs} $<TARGET_OBJECTS:frmt_${frmt}>)
  set_target_properties(frmt_${frmt} PROPERTIES FOLDER "Third Party Libraries/GDAL")
endforeach(frmt ${GDAL_FORMATS})
foreach(frmt ${OGR_FORMATS})
  set(gdal_srcs ${gdal_srcs} $<TARGET_OBJECTS:ogr_frmt_${frmt}>)
  set_target_properties(ogr_frmt_${frmt} PROPERTIES FOLDER "Third Party Libraries/GDAL")
endforeach(frmt ${OGR_FORMATS})

set(gdal_libs ${PROJ4_LIBRARIES} ${PNG_LIBRARIES} ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set(lib_list M DL ICONV)
foreach(ll ${lib_list})
  if(${ll}_LIBRARY)
    set(gdal_libs ${gdal_libs} ${${ll}_LIBRARY})
  endif(${ll}_LIBRARY)
endforeach(ll ${lib_list})
if(MSVC)
  set(gdal_libs ${gdal_libs} ws2_32.lib kernel32.lib psapi.lib)
endif(MSVC)

if (BUILD_SHARED_LIBS)
  add_library(gdal SHARED ${gdal_srcs})
  if(gdal_libs)
    target_link_libraries(gdal ${gdal_libs})
  endif(gdal_libs)
  install(TARGETS gdal
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_SHARED_LIBS)

if (BUILD_STATIC_LIBS)
  add_library(gdal-static STATIC ${gdal_srcs})
  if (MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(gdal-static PROPERTIES PREFIX "lib")
  else (MSVC)
    set_target_properties(gdal-static PROPERTIES OUTPUT_NAME "gdal")
  endif (MSVC)
  install(TARGETS gdal-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

if (BUILD_SHARED_LIBS)
  add_subdirectory(progs)
endif (BUILD_SHARED_LIBS)

# Handle Data files
install(FILES LICENSE.TXT DESTINATION ${DATA_DIR}/gdal)
file(GLOB GDAL_DATA_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} data/*)
install(FILES ${GDAL_DATA_FILES} DESTINATION ${DATA_DIR}/gdal)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

