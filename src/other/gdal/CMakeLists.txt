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
cmake_minimum_required(VERSION 3.0.2)

# Set CMake project name
project(GDAL)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/port
  ${CMAKE_CURRENT_SOURCE_DIR}/gcore
  ${CMAKE_CURRENT_SOURCE_DIR}/alg
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/avc
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/shape
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/mem
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/geojson
  ${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/zlib
  ${CMAKE_CURRENT_SOURCE_DIR}/frmts/iso8211
  ${CMAKE_CURRENT_SOURCE_DIR}/frmts/gtiff/libtiff
  ${CMAKE_CURRENT_SOURCE_DIR}/frmts/gtiff/libgeotiff
  ${CMAKE_CURRENT_SOURCE_DIR}/gnm
  ${CMAKE_CURRENT_SOURCE_DIR}/apps
  )

#-DHAVE_AVX_AT_COMPILE_TIME -DHAVE_SSSE3_AT_COMPILE_TIME -DHAVE_SSE_AT_COMPILE_TIME -g -O2 -Wall -Wextra -Winit-self -Wunused-parameter -Wformat -Werror=format-security -Wno-format-nonliteral -Wlogical-op -Wshadow -Werror=vla -Wdate-time -Wnull-dereference -Wduplicated-cond -Wfloat-conversion -Wmissing-declarations -Wnon-virtual-dtor -Woverloaded-virtual -fno-operator-names -Wsuggest-override -DGNM_ENABLED -I/home/cyapp/brlcad/src/other/gdal/gdal-2.2.1/port -DGDAL_COMPILATION -c genbindataset.cpp  -fPIC -DPIC -o ../o/.libs/genbindataset.o

if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)

find_package(ZLIB)
include_directories(${ZLIB_INCLUDE_DIR})
find_package(PNG)
include_directories(${PNG_INCLUDE_DIR})

add_definitions(-DGDAL_COMPILATION)
add_definitions(-DBIGTIFF_SUPPORT)
add_definitions(-DZLIB_SUPPORT)
add_definitions(-DDONT_DEPRECATE_SPRINTF)
add_definitions(-DGNM_ENABLED)
add_definitions(-DGNMFILE_ENABLED)
add_definitions(-DGNMDB_ENABLED)

if(ZLIB_LIBRARY)
  add_definitions(-DHAVE_ZLIB)
endif(ZLIB_LIBRARY)

include(CheckTypeSize)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)
set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})
check_type_size("long long" HAVE_LONG_LONG)

include(CheckIncludeFiles)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(errno.h HAVE_ERRNO_H)

message("${CMAKE_SYSTEM_PROCESSOR}")
if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "i*86")
  set(HOST_FILLORDER "FILLORDER_LSB2MSB")
else("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "i*86")
  set(HOST_FILLORDER "FILLORDER_MSB2LSB")
endif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "i*86")



set(GDAL_FORMATS
  aaigrid adrg aigrid airsar arg
  blx bmp
  cals ceos ceos2 coasp cosar ctg
  derived dimap dted
  e00grid elas envisat ers
  fit
  gff gsg gtiff gxf
  hf2 hfa
  idrisi ilwis ingr iris
  jaxapalsar jdem kmlsuperoverlay
  l1b leveller
  map mem
  ngsgeoid nitf northwood
  pds prf
  r raw rmf rs2
  safe saga
  sentinel2 sgi srtmhgt
  terragen til tsx
  usgsdem
  vrt
  xpm xyz
  zmap
  )

if(ZLIB_LIBRARY)
  set(GDAL_FORMATS ${GDAL_FORMATS} rik ozi)
endif(ZLIB_LIBRARY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h.tmp)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/cpl_config.h.tmp)


include(port/CMakeLists.txt)
include(gcore/CMakeLists.txt)
include(alg/CMakeLists.txt)
include(frmts/CMakeLists.txt)
include(ogr/CMakeLists.txt)
include(ogr/ogrsf_frmts/CMakeLists.txt)
include(gnm/CMakeLists.txt)
include(apps/CMakeLists.txt)

set(gdal_srcs ${cpl_srcs} ${gcore_srcs} ${alg_srcs} ${frmts_srcs} ${ogr_srcs} ${ogrsf_fmt_srcs} ${gnm_srcs} ${apps_srcs})


if(BUILD_SHARED_LIBS)
  add_library(gdal SHARED ${gdal_srcs})
  target_link_libraries(gdal ${ZLIB_LIBRARY} ${PNG_LIBRARY})
  install(TARGETS gdal
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_SHARED_LIBS)

if(BUILD_STATIC_LIBS)
  add_library(gdal-static STATIC ${gdal_srcs})
  if(NOT WIN32)
    set_target_properties(gdal-static PROPERTIES OUTPUT_NAME "gdal")
  endif(NOT WIN32)
  install(TARGETS gdal-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

