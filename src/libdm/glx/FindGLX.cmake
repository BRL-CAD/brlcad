#                  F I N D G L X . C M A K E
# BRL-CAD
#
# Copyright (c) 2001-2022 United States Government as represented by
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
#
###
# - Try to find X11 OpenGL
# Once done this will define
#
#  XOPENGL_FOUND            - system has OpenGL
#  XOPENGL_INCLUDE_DIR_GL   - the GL include directory
#  XOPENGL_INCLUDE_DIR_GLX  - the GLX include directory
#  XOPENGL_LIBRARIES        - Link these to use OpenGL
#
# If you want to use just GL you can use these values
#  XOPENGL_gl_LIBRARY   - Path to OpenGL Library
#  XOPENGL_glu_LIBRARY  - Path to GLU Library
#
#=============================================================================
# Copyright 2001-2009 Kitware, Inc.
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
# * Neither the names of Kitware, Inc., the Insight Software Consortium,
#   nor the names of their contributors may be used to endorse or promote
#   products derived from this software without specific prior written
#   permission.
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
#
# ------------------------------------------------------------------------------
#
# The above copyright and license notice applies to distributions of
# CMake in source and binary form.  Some source files contain additional
# notices of original copyright by their contributors; see each source
# for details.  Third-party software packages supplied with CMake under
# compatible licenses provide their own copyright notices documented in
# corresponding subdirectories.
#
# ------------------------------------------------------------------------------
#
# CMake was initially developed by Kitware with the following sponsorship:
#
#  * National Library of Medicine at the National Institutes of Health
#    as part of the Insight Segmentation and Registration Toolkit (ITK).
#
#  * US National Labs (Los Alamos, Livermore, Sandia) ASC Parallel
#    Visualization Initiative.
#
#  * National Alliance for Medical Image Computing (NAMIC) is funded by the
#    National Institutes of Health through the NIH Roadmap for Medical Research,
#    Grant U54 EB005149.
#
#  * Kitware, Inc.
#
#=============================================================================

# This file is used in lieu of FindOpenGL.cmake for the glx libdm backend
# because on Mac we need to NOT find the standard OpenGL framework for this
# backend (and ONLY for this backend - if we have native apps not using
# X11, feeding them X11 OpenGL won't work.)

set(XOPENGL_INC_SEARCH_PATH
  /usr/share/doc/NVIDIA_GLX-1.0/include
  /usr/X11/include
  /usr/X11R7/include
  /usr/X11R6/include
  /usr/include/X11
  /usr/local/include/X11
  /usr/local/include
  /usr/include
  /usr/openwin/share/include
  /usr/openwin/include
  /usr/pkg/xorg/include
  /opt/X11/include
  /opt/graphics/OpenGL/include
  )
# Handle HP-UX cases where we only want to find OpenGL in either hpux64
# or hpux32 depending on if we're doing a 64 bit build.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(HPUX_IA_XOPENGL_LIB_PATH /opt/graphics/OpenGL/lib/hpux32/)
else(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(HPUX_IA_XOPENGL_LIB_PATH
    /opt/graphics/OpenGL/lib/hpux64/
    /opt/graphics/OpenGL/lib/pa20_64)
endif(CMAKE_SIZEOF_VOID_P EQUAL 4)

get_property(SEARCH_64BIT GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if(SEARCH_64BIT)
  set(64BIT_DIRS "/usr/lib64/X11;/usr/lib64;/usr/lib/x86_64-linux-gnu")
else(SEARCH_64BIT)
  set(32BIT_DIRS "/usr/lib/X11;/usr/lib;/usr/lib/i386-linux-gnu;/usr/lib/arm-linux-gnueabihf")
endif(SEARCH_64BIT)

set(XOPENGL_LIB_SEARCH_PATH
  ${64BIT_DIRS}
  ${32BIT_DIRS}
  /usr/X11/lib
  /usr/X11R7/lib
  /usr/X11R6/lib
  /usr/shlib
  /usr/openwin/lib
  /opt/X11/lib
  /opt/graphics/OpenGL/lib
  /usr/pkg/xorg/lib
  ${HPUX_IA_XOPENGL_LIB_PATH}
  )

# If we're on Apple and not using Aqua, we don't want frameworks
set(CMAKE_FIND_FRAMEWORK_CACHED "${CMAKE_FIND_FRAMEWORK}")
set(CMAKE_FIND_FRAMEWORK "NEVER")

find_path(XOPENGL_INCLUDE_DIR_GL GL/gl.h        ${XOPENGL_INC_SEARCH_PATH} NO_CMAKE_SYSTEM_PATH)
find_path(XOPENGL_INCLUDE_DIR_GLX GL/glx.h      ${XOPENGL_INC_SEARCH_PATH} NO_CMAKE_SYSTEM_PATH)
find_library(XOPENGL_gl_LIBRARY NAMES GL MesaGL PATHS ${XOPENGL_LIB_SEARCH_PATH} NO_CMAKE_SYSTEM_PATH)
find_library(XOPENGL_gldispatch_LIBRARY NAMES GLdispatch PATHS ${XOPENGL_LIB_SEARCH_PATH} NO_CMAKE_SYSTEM_PATH)

# On Unix OpenGL most certainly always requires X11.
# Feel free to tighten up these conditions if you don't
# think this is always true.
# It's not true on OSX.

if(XOPENGL_gl_LIBRARY)
  if(NOT X11_FOUND)
    include(FindX11)
  endif(NOT X11_FOUND)
  if(X11_FOUND)
    set(XOPENGL_LIBRARIES ${X11_LIBRARIES})
  endif(X11_FOUND)
endif(XOPENGL_gl_LIBRARY)

find_library(XOPENGL_glu_LIBRARY NAMES GLU MesaGLU PATHS ${XOPENGL_LIB_SEARCH_PATH} NO_CMAKE_SYSTEM_PATH)

set(CMAKE_FIND_FRAMEWORK "${CMAKE_FIND_FRAMEWORK_CACHED}")

if(XOPENGL_INCLUDE_DIR_GLX AND XOPENGL_INCLUDE_DIR_GL AND XOPENGL_gl_LIBRARY)
  set(XOPENGL_FOUND "YES" )
endif(XOPENGL_INCLUDE_DIR_GLX AND XOPENGL_INCLUDE_DIR_GL AND XOPENGL_gl_LIBRARY)

mark_as_advanced(
  XOPENGL_INCLUDE_DIR_GL
  XOPENGL_INCLUDE_DIR_GLX
  XOPENGL_glu_LIBRARY
  XOPENGL_gl_LIBRARY
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
