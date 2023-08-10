# Copyright (c) 1993-2015 Ken Martin, Will Schroeder, Bill Lorensen
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
#  * Neither name of Ken Martin, Will Schroeder, or Bill Lorensen nor the names
#    of any contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_path(GL2PS_INCLUDE_DIR
  NAMES gl2ps.h
  DOC "gl2ps include directories")
mark_as_advanced(GL2PS_INCLUDE_DIR)

find_library(GL2PS_LIBRARY
  NAMES gl2ps
  DOC "gl2ps library")
mark_as_advanced(GL2PS_LIBRARY)

if (GL2PS_INCLUDE_DIR)
  file(STRINGS "${GL2PS_INCLUDE_DIR}/gl2ps.h" _gl2ps_version_lines REGEX "#define[ \t]+GL2PS_(MAJOR|MINOR|PATCH)_VERSION[ \t]+")
  string(REGEX REPLACE ".*GL2PS_MAJOR_VERSION *\([0-9]*\).*" "\\1" _gl2ps_version_major "${_gl2ps_version_lines}")
  string(REGEX REPLACE ".*GL2PS_MINOR_VERSION *\([0-9]*\).*" "\\1" _gl2ps_version_minor "${_gl2ps_version_lines}")
  string(REGEX REPLACE ".*GL2PS_PATCH_VERSION *\([0-9]*\).*" "\\1" _gl2ps_version_patch "${_gl2ps_version_lines}")
  set(GL2PS_VERSION "${_gl2ps_version_major}.${_gl2ps_version_minor}.${_gl2ps_version_patch}")
  unset(_gl2ps_version_major)
  unset(_gl2ps_version_minor)
  unset(_gl2ps_version_patch)
  unset(_gl2ps_version_lines)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GL2PS
  REQUIRED_VARS GL2PS_LIBRARY GL2PS_INCLUDE_DIR
  VERSION_VAR GL2PS_VERSION)

if (GL2PS_FOUND)
  set(GL2PS_INCLUDE_DIRS "${GL2PS_INCLUDE_DIR}")
  set(GL2PS_LIBRARIES "${GL2PS_LIBRARY}")

  if (NOT TARGET GL2PS::GL2PS)
    add_library(GL2PS::GL2PS UNKNOWN IMPORTED)
    set_target_properties(GL2PS::GL2PS PROPERTIES
      IMPORTED_LOCATION "${GL2PS_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${GL2PS_INCLUDE_DIR}")
  endif ()
endif ()


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
