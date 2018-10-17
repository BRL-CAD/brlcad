# Copyright 2000-2017 Kitware, Inc. and Contributors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# * Neither the name of Kitware, Inc. nor the names of Contributors
# may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#.rst:
# FindGDAL
# --------
#
# Locate gdal
#
# This module accepts the following environment variables:
#
# ::
#
#     GDAL_DIR or GDAL_ROOT - Specify the location of GDAL
#
#
#
# This module defines the following CMake variables:
#
# ::
#
#     GDAL_FOUND - True if libgdal is found
#     GDAL_LIBRARY - A variable pointing to the GDAL library
#     GDAL_INCLUDE_DIR - Where to find the headers

#
# $GDALDIR is an environment variable that would
# correspond to the ./configure --prefix=$GDAL_DIR
# used in building gdal.
#
# Created by Eric Wing. I'm not a gdal user, but OpenSceneGraph uses it
# for osgTerrain so I whipped this module together for completeness.
# I actually don't know the conventions or where files are typically
# placed in distros.
# Any real gdal users are encouraged to correct this (but please don't
# break the OS X framework stuff when doing so which is what usually seems
# to happen).

# This makes the presumption that you are include gdal.h like
#
#include "gdal.h"

include(CheckCSourceCompiles)
function(GDALTRANS_TEST HGDALTRANS)

  set(CMAKE_REQUIRED_INCLUDES ${GDAL_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${GDAL_LIBRARY})

  set(GT_SRCS "
  #include <gdal.h>
  #include <gdalwarper.h>
  #include <gdal_utils.h>
  #include <cpl_conv.h>
  #include <cpl_string.h>
  #include <cpl_multiproc.h>
  #include <ogr_spatialref.h>
  #include <vrtdataset.h>

  int main(int ac, char *av[])
  {
  GDALDatasetH gd = GDALTranslate(NULL, NULL, NULL, NULL);
  }
  ")

  CHECK_C_SOURCE_COMPILES("${GT_SRCS}" H_GT)

  set(${HGDALTRANS} ${H_GT} PARENT_SCOPE)

endfunction(GDALTRANS_TEST)


find_path(GDAL_INCLUDE_DIR gdal.h
  HINTS
  ENV GDAL_DIR
  ENV GDAL_ROOT
  PATH_SUFFIXES
  include/gdal
  include/GDAL
  include
  PATHS
  ~/Library/Frameworks/gdal.framework/Headers
  /Library/Frameworks/gdal.framework/Headers
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt
  )

# Use gdal-config to obtain the library version (this should hopefully
# allow us to -lgdal1.x.y where x.y are correct version)
# For some reason, libgdal development packages do not contain
# libgdal.so...
find_program(GDAL_CONFIG gdal-config
  HINTS
  ENV GDAL_DIR
  ENV GDAL_ROOT
  PATH_SUFFIXES bin
  PATHS
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt
  )

if(GDAL_CONFIG AND NOT "${GDAL_CONFIG}" MATCHES "NOTFOUND")
  exec_program(${GDAL_CONFIG} ARGS --libs OUTPUT_VARIABLE GDAL_CONFIG_LIBS)
  if(GDAL_CONFIG_LIBS)
    string(REGEX MATCHALL "-l[^ ]+" _gdal_dashl ${GDAL_CONFIG_LIBS})
    string(REPLACE "-l" "" _gdal_lib "${_gdal_dashl}")
    string(REGEX MATCHALL "-L[^ ]+" _gdal_dashL ${GDAL_CONFIG_LIBS})
    string(REPLACE "-L" "" _gdal_libpath "${_gdal_dashL}")
  endif(GDAL_CONFIG_LIBS)
endif(GDAL_CONFIG AND NOT "${GDAL_CONFIG}" MATCHES "NOTFOUND")

find_library(GDAL_LIBRARY
  NAMES ${_gdal_lib} gdal GDAL
  HINTS
  ENV GDAL_DIR
  ENV GDAL_ROOT
  ${_gdal_libpath}
  PATH_SUFFIXES lib
  PATHS
  /sw
  /opt/local
  /opt/csw
  /opt
  /usr/freeware
  )

# Check for GDALTranslate - if we don't have that, we're not in business
if(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)
  GDALTRANS_TEST(HAVE_GDALTRANSLATE)
  if(NOT HAVE_GDALTRANSLATE)
    set(GDAL_LIBRARY NOTFOUND)
    set(GDAL_INCLUDE_DIR "")
  endif(NOT HAVE_GDALTRANSLATE)
endif(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GDAL DEFAULT_MSG GDAL_LIBRARY GDAL_INCLUDE_DIR HAVE_GDALTRANSLATE)

if(HAVE_GDALTRANSLATE)
  set(GDAL_LIBRARIES ${GDAL_LIBRARY})
  set(GDAL_INCLUDE_DIRS ${GDAL_INCLUDE_DIR})
endif(HAVE_GDALTRANSLATE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

