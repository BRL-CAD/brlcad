# Distributed under the OSI-approved BSD 3-Clause License.
#
# Copyright 2000-2021 Kitware, Inc. and Contributors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Kitware, Inc. nor the names of Contributors
#   may be used to endorse or promote products derived from this
#   software without specific prior written permission.
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

#[=======================================================================[.rst:
FindLibUV
---------

Find libuv includes and library.

Imported Targets
^^^^^^^^^^^^^^^^

An :ref:`imported target <Imported targets>` named
``LibUV::LibUV`` is provided if libuv has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``LibUV_FOUND``
  True if libuv was found, false otherwise.
``LibUV_INCLUDE_DIRS``
  Include directories needed to include libuv headers.
``LibUV_LIBRARIES``
  Libraries needed to link to libuv.
``LibUV_VERSION``
  The version of libuv found.
``LibUV_VERSION_MAJOR``
  The major version of libuv.
``LibUV_VERSION_MINOR``
  The minor version of libuv.
``LibUV_VERSION_PATCH``
  The patch version of libuv.

Cache Variables
^^^^^^^^^^^^^^^

This module uses the following cache variables:

``LibUV_LIBRARY``
  The location of the libuv library file.
``LibUV_INCLUDE_DIR``
  The location of the libuv include directory containing ``uv.h``.

The cache variables should not be used by project code.
They may be set by end users to point at libuv components.
#]=======================================================================]

#-----------------------------------------------------------------------------
find_library(LibUV_LIBRARY
  NAMES uv libuv
  )
mark_as_advanced(LibUV_LIBRARY)

find_path(LibUV_INCLUDE_DIR
  NAMES uv.h
  )
mark_as_advanced(LibUV_INCLUDE_DIR)

#-----------------------------------------------------------------------------
# Extract version number if possible.
set(_LibUV_H_REGEX "#[ \t]*define[ \t]+UV_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")
if(LibUV_INCLUDE_DIR AND EXISTS "${LibUV_INCLUDE_DIR}/uv-version.h")
  file(STRINGS "${LibUV_INCLUDE_DIR}/uv-version.h" _LibUV_H REGEX "${_LibUV_H_REGEX}")
elseif(LibUV_INCLUDE_DIR AND EXISTS "${LibUV_INCLUDE_DIR}/uv/version.h")
  file(STRINGS "${LibUV_INCLUDE_DIR}/uv/version.h" _LibUV_H REGEX "${_LibUV_H_REGEX}")
elseif(LibUV_INCLUDE_DIR AND EXISTS "${LibUV_INCLUDE_DIR}/uv.h")
  file(STRINGS "${LibUV_INCLUDE_DIR}/uv.h" _LibUV_H REGEX "${_LibUV_H_REGEX}")
else()
  set(_LibUV_H "")
endif()
foreach(c MAJOR MINOR PATCH)
  if(_LibUV_H MATCHES "#[ \t]*define[ \t]+UV_VERSION_${c}[ \t]+([0-9]+)")
    set(_LibUV_VERSION_${c} "${CMAKE_MATCH_1}")
  else()
    unset(_LibUV_VERSION_${c})
  endif()
endforeach()
if(DEFINED _LibUV_VERSION_MAJOR AND DEFINED _LibUV_VERSION_MINOR)
  set(LibUV_VERSION_MAJOR "${_LibUV_VERSION_MAJOR}")
  set(LibUV_VERSION_MINOR "${_LibUV_VERSION_MINOR}")
  set(LibUV_VERSION "${LibUV_VERSION_MAJOR}.${LibUV_VERSION_MINOR}")
  if(DEFINED _LibUV_VERSION_PATCH)
    set(LibUV_VERSION_PATCH "${_LibUV_VERSION_PATCH}")
    set(LibUV_VERSION "${LibUV_VERSION}.${LibUV_VERSION_PATCH}")
  else()
    unset(LibUV_VERSION_PATCH)
  endif()
else()
  set(LibUV_VERSION_MAJOR "")
  set(LibUV_VERSION_MINOR "")
  set(LibUV_VERSION_PATCH "")
  set(LibUV_VERSION "")
endif()
unset(_LibUV_VERSION_MAJOR)
unset(_LibUV_VERSION_MINOR)
unset(_LibUV_VERSION_PATCH)
unset(_LibUV_H_REGEX)
unset(_LibUV_H)

#-----------------------------------------------------------------------------
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibUV
  FOUND_VAR LibUV_FOUND
  REQUIRED_VARS LibUV_LIBRARY LibUV_INCLUDE_DIR
  VERSION_VAR LibUV_VERSION
  )
set(LIBUV_FOUND ${LibUV_FOUND})

#-----------------------------------------------------------------------------
# Provide documented result variables and targets.
if(LibUV_FOUND)
  set(LibUV_INCLUDE_DIRS ${LibUV_INCLUDE_DIR})
  set(LibUV_LIBRARIES ${LibUV_LIBRARY})
  if(NOT TARGET LibUV::LibUV)
    add_library(LibUV::LibUV UNKNOWN IMPORTED)
    set_target_properties(LibUV::LibUV PROPERTIES
      IMPORTED_LOCATION "${LibUV_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${LibUV_INCLUDE_DIRS}"
      )
  endif()
endif()
