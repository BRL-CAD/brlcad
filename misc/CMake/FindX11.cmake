#                   F I N D X 1 1 . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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
# - Find X11 installation
# Try to find X11 on UNIX systems. The following values are defined
#  X11_FOUND        - True if X11 is available
#  X11_INCLUDE_DIR  - include directories to use X11
#  X11_LIBRARIES    - link against these to use X11
#
# and also the following more fine grained variables:
# Include paths: X11_ICE_INCLUDE_PATH,          X11_ICE_LIB,        X11_ICE_FOUND
#                X11_X11_INCLUDE_PATH,          X11_X11_LIB
#                X11_Xaccessrules_INCLUDE_PATH,                     X11_Xaccess_FOUND
#                X11_Xaccessstr_INCLUDE_PATH,                       X11_Xaccess_FOUND
#                X11_Xau_INCLUDE_PATH,          X11_Xau_LIB,        X11_Xau_FOUND
#                X11_Xcomposite_INCLUDE_PATH,   X11_Xcomposite_LIB, X11_Xcomposite_FOUND
#                X11_Xcursor_INCLUDE_PATH,      X11_Xcursor_LIB,    X11_Xcursor_FOUND
#                X11_Xdamage_INCLUDE_PATH,      X11_Xdamage_LIB,    X11_Xdamage_FOUND
#                X11_Xdmcp_INCLUDE_PATH,        X11_Xdmcp_LIB,      X11_Xdmcp_FOUND
#                                               X11_Xext_LIB,       X11_Xext_FOUND
#                X11_dpms_INCLUDE_PATH,         (in X11_Xext_LIB),  X11_dpms_FOUND
#                X11_XShm_INCLUDE_PATH,         (in X11_Xext_LIB),  X11_XShm_FOUND
#                X11_Xshape_INCLUDE_PATH,       (in X11_Xext_LIB),  X11_Xshape_FOUND
#                X11_xf86misc_INCLUDE_PATH,     X11_Xxf86misc_LIB,  X11_xf86misc_FOUND
#                X11_xf86vmode_INCLUDE_PATH,                        X11_xf86vmode_FOUND
#                X11_Xfixes_INCLUDE_PATH,       X11_Xfixes_LIB,     X11_Xfixes_FOUND
#                X11_Xft_INCLUDE_PATH,          X11_Xft_LIB,        X11_Xft_FOUND
#                X11_Xi_INCLUDE_PATH,           X11_Xi_LIB,         X11_Xi_FOUND
#                X11_Xinerama_INCLUDE_PATH,     X11_Xinerama_LIB,   X11_Xinerama_FOUND
#                X11_Xinput_INCLUDE_PATH,       X11_Xinput_LIB,     X11_Xinput_FOUND
#                X11_Xkb_INCLUDE_PATH,                              X11_Xkb_FOUND
#                X11_Xkblib_INCLUDE_PATH,                           X11_Xkb_FOUND
#                X11_Xpm_INCLUDE_PATH,          X11_Xpm_LIB,        X11_Xpm_FOUND
#                X11_XTest_INCLUDE_PATH,        X11_XTest_LIB,      X11_XTest_FOUND
#                X11_Xrandr_INCLUDE_PATH,       X11_Xrandr_LIB,     X11_Xrandr_FOUND
#                X11_Xrender_INCLUDE_PATH,      X11_Xrender_LIB,    X11_Xrender_FOUND
#                X11_Xscreensaver_INCLUDE_PATH, X11_Xscreensaver_LIB, X11_Xscreensaver_FOUND
#                X11_Xt_INCLUDE_PATH,           X11_Xt_LIB,         X11_Xt_FOUND
#                X11_Xutil_INCLUDE_PATH,                            X11_Xutil_FOUND
#                X11_Xv_INCLUDE_PATH,           X11_Xv_LIB,         X11_Xv_FOUND

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

MACRO(X11_FIND_INCLUDE_PATH component header)
  find_path(X11_${component}_INCLUDE_PATH ${header} ${X11_INC_SEARCH_PATH} NO_CMAKE_SYSTEM_PATH)
  set(X11_HDR_VARS ${X11_HDR_VARS} X11_${component}_INCLUDE_PATH)
  if(X11_${component}_INCLUDE_PATH)
    set(X11_HDR_PATHS ${X11_HDR_PATHS} ${X11_${component}_INCLUDE_PATH})
    list(REMOVE_DUPLICATES X11_HDR_PATHS)
  endif(X11_${component}_INCLUDE_PATH)
ENDMACRO(X11_FIND_INCLUDE_PATH)

MACRO(X11_FIND_LIB_PATH component libname)
  find_library(X11_${component}_LIB ${libname} ${X11_LIB_SEARCH_PATH} NO_CMAKE_SYSTEM_PATH)
  set(X11_LIB_VARS ${X11_LIB_VARS} X11_${component}_LIB)
  if(X11_${component}_LIB)
    get_filename_component(X11_${component}_DIR ${X11_${component}_LIB} PATH)
    set(X11_LIB_PATHS ${X11_LIB_PATHS} ${X11_${component}_DIR})
    list(REMOVE_DUPLICATES X11_LIB_PATHS)
  endif(X11_${component}_LIB)
ENDMACRO(X11_FIND_LIB_PATH)

if(UNIX)
  set(X11_FOUND 0)
  # X11 is never a framework and some header files may be
  # found in tcl on the mac
  set(CMAKE_FIND_FRAMEWORK_SAVE ${CMAKE_FIND_FRAMEWORK})
  set(CMAKE_FIND_FRAMEWORK NEVER)


  # See whether we're looking for 32 or 64 bit libraries,
  # and organize our search directories accordingly.  The
  # common convention is to use lib64 for 64 bit versions of
  # libraries, but some distributions (notably archlinux)
  # use lib32 for 32 bit and lib for 64 bit.
  get_property(SEARCH_64BIT GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
  if(SEARCH_64BIT)
    set(64BIT_DIRS "/usr/lib64/X11;/usr/lib64;/usr/lib/x86_64-linux-gnu")
    if(EXISTS "/usr/lib32" OR NOT EXISTS "/usr/lib64")
      set(64BIT_DIRS ${64BIT_DIRS} /usr/lib/X11 /usr/lib)
    endif(EXISTS "/usr/lib32" OR NOT EXISTS "/usr/lib64")
  else(SEARCH_64BIT)
    set(32BIT_DIRS "/usr/lib32/X11;/usr/lib32;/usr/lib/i386-linux-gnu")
    if(EXISTS "/usr/lib64" OR NOT EXISTS "/usr/lib32")
      set(32BIT_DIRS ${32BIT_DIRS} /usr/lib/X11 /usr/lib)
    endif(EXISTS "/usr/lib64" OR NOT EXISTS "/usr/lib32")
  endif(SEARCH_64BIT)

  # Candidate directories for headers
  set(X11_INC_SEARCH_PATH
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
    /opt/graphics/OpenGL/include
    )

  # Candidate directories for libraries.
  set(X11_LIB_SEARCH_PATH
    ${64BIT_DIRS}
    ${32BIT_DIRS}
    /usr/X11/lib
    /usr/X11R7/lib
    /usr/X11R6/lib
    /usr/openwin/lib
    /usr/pkg/xorg/lib
    )

  # Just in case, clear our lists
  set(X11_HDR_VARS)
  set(X11_HDR_PATHS)
  set(X11_LIB_VARS)
  set(X11_LIB_PATHS)

  # Find primary X11 headers
  X11_FIND_INCLUDE_PATH(X11		X11/X.h)
  X11_FIND_INCLUDE_PATH(Xlib		X11/Xlib.h)
  # Look for other X11 includes; keep the list sorted by name of the cmake X11_<component>_INCLUDE_PATH
  # variable (component doesn't need to match the include file name).
  X11_FIND_INCLUDE_PATH(ICE 		X11/ICE/ICE.h)
  X11_FIND_INCLUDE_PATH(Xaccessrules 	X11/extensions/XKBrules.h)
  X11_FIND_INCLUDE_PATH(Xaccessstr 	X11/extensions/XKBstr.h)
  X11_FIND_INCLUDE_PATH(Xau 		X11/Xauth.h)
  X11_FIND_INCLUDE_PATH(Xcomposite 	X11/extensions/Xcomposite.h)
  X11_FIND_INCLUDE_PATH(Xcursor 	X11/Xcursor/Xcursor.h)
  X11_FIND_INCLUDE_PATH(Xdamage 	X11/extensions/Xdamage.h)
  X11_FIND_INCLUDE_PATH(Xdmcp 		X11/Xdmcp.h)
  X11_FIND_INCLUDE_PATH(dpms 		X11/extensions/dpms.h)
  X11_FIND_INCLUDE_PATH(xf86misc 	X11/extensions/xf86misc.h)
  X11_FIND_INCLUDE_PATH(xf86vmode 	X11/extensions/xf86vmode.h)
  X11_FIND_INCLUDE_PATH(Xfixes		X11/extensions/Xfixes.h)
  X11_FIND_INCLUDE_PATH(Xft		X11/Xft/Xft.h)
  X11_FIND_INCLUDE_PATH(Xi		X11/extensions/XInput.h)
  X11_FIND_INCLUDE_PATH(Xinerama	X11/extensions/Xinerama.h)
  X11_FIND_INCLUDE_PATH(Xinput		X11/extensions/XInput.h)
  X11_FIND_INCLUDE_PATH(Xkb		X11/extensions/XKB.h)
  X11_FIND_INCLUDE_PATH(Xkblib		X11/XKBlib.h)
  X11_FIND_INCLUDE_PATH(Xpm		X11/xpm.h)
  X11_FIND_INCLUDE_PATH(XTest		X11/extensions/XTest.h)
  X11_FIND_INCLUDE_PATH(XShm		X11/extensions/XShm.h)
  X11_FIND_INCLUDE_PATH(Xrandr		X11/extensions/Xrandr.h)
  X11_FIND_INCLUDE_PATH(Xrender		X11/extensions/Xrender.h)
  X11_FIND_INCLUDE_PATH(Xscreensaver	X11/extensions/scrnsaver.h)
  X11_FIND_INCLUDE_PATH(Xshape		X11/extensions/shape.h)
  X11_FIND_INCLUDE_PATH(Xutil		X11/Xutil.h)
  X11_FIND_INCLUDE_PATH(Xt		X11/Intrinsic.h)
  X11_FIND_INCLUDE_PATH(Xv		X11/extensions/Xvlib.h)


  # Find primary X11 library
  X11_FIND_LIB_PATH(X11			X11)
  # Find additional X libraries. Keep list sorted by library name.
  X11_FIND_LIB_PATH(ICE			ICE)
  X11_FIND_LIB_PATH(SM			SM)
  X11_FIND_LIB_PATH(Xau			Xau)
  X11_FIND_LIB_PATH(Xcomposite		Xcomposite)
  X11_FIND_LIB_PATH(Xcursor		Xcursor)
  X11_FIND_LIB_PATH(Xdamage		Xdamage)
  X11_FIND_LIB_PATH(Xdmcp		Xdmcp)
  X11_FIND_LIB_PATH(Xext		Xext)
  X11_FIND_LIB_PATH(Xfixes		Xfixes)
  X11_FIND_LIB_PATH(Xft			Xft)
  X11_FIND_LIB_PATH(Xi			Xi)
  X11_FIND_LIB_PATH(Xinerama		Xinerama)
  X11_FIND_LIB_PATH(Xpm			Xpm)
  X11_FIND_LIB_PATH(Xrandr		Xrandr)
  X11_FIND_LIB_PATH(Xrender		Xrender)
  X11_FIND_LIB_PATH(Xscreensaver	Xss)
  X11_FIND_LIB_PATH(Xt			Xt)
  X11_FIND_LIB_PATH(XTest		Xtst)
  X11_FIND_LIB_PATH(Xv			Xv)
  X11_FIND_LIB_PATH(Xxf86misc		Xxf86misc)

  set(X11_LIBRARY_DIR "")
  if(X11_X11_LIB)
    get_filename_component(X11_LIBRARY_DIR ${X11_X11_LIB} PATH)
  endif(X11_X11_LIB)

  set(X11_INCLUDE_DIR) # start with empty list
  if(X11_X11_INCLUDE_PATH)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_X11_INCLUDE_PATH})
  endif(X11_X11_INCLUDE_PATH)

  if(X11_Xlib_INCLUDE_PATH)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xlib_INCLUDE_PATH})
  endif(X11_Xlib_INCLUDE_PATH)

  if(X11_Xutil_INCLUDE_PATH)
    set(X11_Xutil_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xutil_INCLUDE_PATH})
  endif(X11_Xutil_INCLUDE_PATH)

  if(X11_Xshape_INCLUDE_PATH)
    set(X11_Xshape_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xshape_INCLUDE_PATH})
  endif(X11_Xshape_INCLUDE_PATH)

  # We'll need a variable containing all X11 libraries found
  set(X11_LIBRARIES) # start with empty list

  if(X11_X11_LIB)
    set(X11_LIBRARIES ${X11_LIBRARIES} ${X11_X11_LIB})
  endif(X11_X11_LIB)

  if(X11_Xext_LIB)
    set(X11_Xext_FOUND TRUE)
    set(X11_LIBRARIES ${X11_LIBRARIES} ${X11_Xext_LIB})
  endif(X11_Xext_LIB)

  if(X11_Xt_LIB AND X11_Xt_INCLUDE_PATH)
    set(X11_Xt_FOUND TRUE)
  endif(X11_Xt_LIB AND X11_Xt_INCLUDE_PATH)

  if(X11_Xft_LIB AND X11_Xft_INCLUDE_PATH)
    set(X11_Xft_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xft_INCLUDE_PATH})
  endif(X11_Xft_LIB AND X11_Xft_INCLUDE_PATH)

  if(X11_Xv_LIB AND X11_Xv_INCLUDE_PATH)
    set(X11_Xv_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xv_INCLUDE_PATH})
  endif(X11_Xv_LIB AND X11_Xv_INCLUDE_PATH)

  if(X11_Xau_LIB AND X11_Xau_INCLUDE_PATH)
    set(X11_Xau_FOUND TRUE)
  endif(X11_Xau_LIB AND X11_Xau_INCLUDE_PATH)

  if(X11_Xdmcp_INCLUDE_PATH AND X11_Xdmcp_LIB)
    set(X11_Xdmcp_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xdmcp_INCLUDE_PATH})
  endif(X11_Xdmcp_INCLUDE_PATH AND X11_Xdmcp_LIB)

  if(X11_Xaccessrules_INCLUDE_PATH AND X11_Xaccessstr_INCLUDE_PATH)
    set(X11_Xaccess_FOUND TRUE)
    set(X11_Xaccess_INCLUDE_PATH ${X11_Xaccessstr_INCLUDE_PATH})
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xaccess_INCLUDE_PATH})
  endif(X11_Xaccessrules_INCLUDE_PATH AND X11_Xaccessstr_INCLUDE_PATH)

  if(X11_Xpm_INCLUDE_PATH AND X11_Xpm_LIB)
    set(X11_Xpm_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xpm_INCLUDE_PATH})
  endif(X11_Xpm_INCLUDE_PATH AND X11_Xpm_LIB)

  if(X11_Xcomposite_INCLUDE_PATH AND X11_Xcomposite_LIB)
    set(X11_Xcomposite_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xcomposite_INCLUDE_PATH})
  endif(X11_Xcomposite_INCLUDE_PATH AND X11_Xcomposite_LIB)

  if(X11_Xdamage_INCLUDE_PATH AND X11_Xdamage_LIB)
    set(X11_Xdamage_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xdamage_INCLUDE_PATH})
  endif(X11_Xdamage_INCLUDE_PATH AND X11_Xdamage_LIB)

  if(X11_XShm_INCLUDE_PATH)
    set(X11_XShm_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_XShm_INCLUDE_PATH})
  endif(X11_XShm_INCLUDE_PATH)

  if(X11_XTest_INCLUDE_PATH AND X11_XTest_LIB)
    set(X11_XTest_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_XTest_INCLUDE_PATH})
  endif(X11_XTest_INCLUDE_PATH AND X11_XTest_LIB)

  if(X11_Xi_INCLUDE_PATH AND X11_Xi_LIB)
    set(X11_Xi_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xi_INCLUDE_PATH})
  endif(X11_Xi_INCLUDE_PATH  AND X11_Xi_LIB)

  if(X11_Xinerama_INCLUDE_PATH AND X11_Xinerama_LIB)
    set(X11_Xinerama_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xinerama_INCLUDE_PATH})
  endif(X11_Xinerama_INCLUDE_PATH  AND X11_Xinerama_LIB)

  if(X11_Xfixes_INCLUDE_PATH AND X11_Xfixes_LIB)
    set(X11_Xfixes_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xfixes_INCLUDE_PATH})
  endif(X11_Xfixes_INCLUDE_PATH AND X11_Xfixes_LIB)

  if(X11_Xrender_INCLUDE_PATH AND X11_Xrender_LIB)
    set(X11_Xrender_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xrender_INCLUDE_PATH})
  endif(X11_Xrender_INCLUDE_PATH AND X11_Xrender_LIB)

  if(X11_Xrandr_INCLUDE_PATH AND X11_Xrandr_LIB)
    set(X11_Xrandr_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})
  endif(X11_Xrandr_INCLUDE_PATH AND X11_Xrandr_LIB)

  if(X11_xf86misc_INCLUDE_PATH AND X11_Xxf86misc_LIB)
    set(X11_xf86misc_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_xf86misc_INCLUDE_PATH})
  endif(X11_xf86misc_INCLUDE_PATH  AND X11_Xxf86misc_LIB)

  if(X11_xf86vmode_INCLUDE_PATH)
    set(X11_xf86vmode_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_xf86vmode_INCLUDE_PATH})
  endif(X11_xf86vmode_INCLUDE_PATH)

  if(X11_Xcursor_INCLUDE_PATH AND X11_Xcursor_LIB)
    set(X11_Xcursor_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xcursor_INCLUDE_PATH})
  endif(X11_Xcursor_INCLUDE_PATH AND X11_Xcursor_LIB)

  if(X11_Xscreensaver_INCLUDE_PATH AND X11_Xscreensaver_LIB)
    set(X11_Xscreensaver_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xscreensaver_INCLUDE_PATH})
  endif(X11_Xscreensaver_INCLUDE_PATH AND X11_Xscreensaver_LIB)

  if(X11_dpms_INCLUDE_PATH)
    set(X11_dpms_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_dpms_INCLUDE_PATH})
  endif(X11_dpms_INCLUDE_PATH)

  if(X11_Xkb_INCLUDE_PATH AND X11_Xkblib_INCLUDE_PATH AND X11_Xlib_INCLUDE_PATH)
    set(X11_Xkb_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xkb_INCLUDE_PATH} )
  endif(X11_Xkb_INCLUDE_PATH AND X11_Xkblib_INCLUDE_PATH AND X11_Xlib_INCLUDE_PATH)

  if(X11_Xinput_INCLUDE_PATH AND X11_Xinput_LIB)
    set(X11_Xinput_FOUND TRUE)
    set(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xinput_INCLUDE_PATH})
  endif(X11_Xinput_INCLUDE_PATH AND X11_Xinput_LIB)

  if(X11_ICE_LIB AND X11_ICE_INCLUDE_PATH)
    set(X11_ICE_FOUND TRUE)
  endif(X11_ICE_LIB AND X11_ICE_INCLUDE_PATH)

  # Deprecated variable for backwards compatibility with CMake 1.4
  if(X11_Xlib_INCLUDE_PATH AND X11_LIBRARIES)
    set(X11_FOUND 1)
  endif(X11_Xlib_INCLUDE_PATH AND X11_LIBRARIES)

  if(X11_FOUND)
    include(CheckFunctionExists)
    include(CheckLibraryExists)

    # Translated from an autoconf-generated configure script.
    # See libs.m4 in autoconf's m4 directory.
    if($ENV{ISC} MATCHES "^yes$")
      set(X11_X_EXTRA_LIBS -lnsl_s -linet)
    else($ENV{ISC} MATCHES "^yes$")
      set(X11_X_EXTRA_LIBS "")

      # See if XOpenDisplay in X11 works by itself.
      CHECK_LIBRARY_EXISTS("${X11_LIBRARIES}" "XOpenDisplay" "${X11_LIBRARY_DIR}" X11_LIB_X11_SOLO)
      if(NOT X11_LIB_X11_SOLO)
        # Find library needed for dnet_ntoa.
        CHECK_LIBRARY_EXISTS("dnet" "dnet_ntoa" "" X11_LIB_DNET_HAS_DNET_NTOA)
        if(X11_LIB_DNET_HAS_DNET_NTOA)
          SET (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -ldnet)
        else(X11_LIB_DNET_HAS_DNET_NTOA)
          CHECK_LIBRARY_EXISTS("dnet_stub" "dnet_ntoa" "" X11_LIB_DNET_STUB_HAS_DNET_NTOA)
          if(X11_LIB_DNET_STUB_HAS_DNET_NTOA)
            SET (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -ldnet_stub)
          endif(X11_LIB_DNET_STUB_HAS_DNET_NTOA)
        endif(X11_LIB_DNET_HAS_DNET_NTOA)
      endif(NOT X11_LIB_X11_SOLO)

      # Find library needed for gethostbyname.
      CHECK_FUNCTION_EXISTS("gethostbyname" CMAKE_HAVE_GETHOSTBYNAME)
      if(NOT CMAKE_HAVE_GETHOSTBYNAME)
        CHECK_LIBRARY_EXISTS("nsl" "gethostbyname" "" CMAKE_LIB_NSL_HAS_GETHOSTBYNAME)
        if(CMAKE_LIB_NSL_HAS_GETHOSTBYNAME)
          SET (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lnsl)
        else(CMAKE_LIB_NSL_HAS_GETHOSTBYNAME)
          CHECK_LIBRARY_EXISTS("bsd" "gethostbyname" "" CMAKE_LIB_BSD_HAS_GETHOSTBYNAME)
          if(CMAKE_LIB_BSD_HAS_GETHOSTBYNAME)
            SET (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lbsd)
          endif(CMAKE_LIB_BSD_HAS_GETHOSTBYNAME)
        endif(CMAKE_LIB_NSL_HAS_GETHOSTBYNAME)
      endif(NOT CMAKE_HAVE_GETHOSTBYNAME)

      # Find library needed for connect.
      CHECK_FUNCTION_EXISTS("connect" CMAKE_HAVE_CONNECT)
      if(NOT CMAKE_HAVE_CONNECT)
        CHECK_LIBRARY_EXISTS("socket" "connect" "" CMAKE_LIB_SOCKET_HAS_CONNECT)
        if(CMAKE_LIB_SOCKET_HAS_CONNECT)
          SET (X11_X_EXTRA_LIBS -lsocket ${X11_X_EXTRA_LIBS})
        endif(CMAKE_LIB_SOCKET_HAS_CONNECT)
      endif(NOT CMAKE_HAVE_CONNECT)

      # Find library needed for remove.
      CHECK_FUNCTION_EXISTS("remove" CMAKE_HAVE_REMOVE)
      if(NOT CMAKE_HAVE_REMOVE)
        CHECK_LIBRARY_EXISTS("posix" "remove" "" CMAKE_LIB_POSIX_HAS_REMOVE)
        if(CMAKE_LIB_POSIX_HAS_REMOVE)
          SET (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lposix)
        endif(CMAKE_LIB_POSIX_HAS_REMOVE)
      endif(NOT CMAKE_HAVE_REMOVE)

      # Find library needed for shmat.
      CHECK_FUNCTION_EXISTS("shmat" CMAKE_HAVE_SHMAT)
      if(NOT CMAKE_HAVE_SHMAT)
        CHECK_LIBRARY_EXISTS("ipc" "shmat" "" CMAKE_LIB_IPS_HAS_SHMAT)
        if(CMAKE_LIB_IPS_HAS_SHMAT)
          SET (X11_X_EXTRA_LIBS ${X11_X_EXTRA_LIBS} -lipc)
        endif(CMAKE_LIB_IPS_HAS_SHMAT)
      endif(NOT CMAKE_HAVE_SHMAT)
    endif($ENV{ISC} MATCHES "^yes$")

    if(X11_ICE_FOUND)
      CHECK_LIBRARY_EXISTS("ICE" "IceConnectionNumber" "${X11_LIBRARY_DIR}"
        CMAKE_LIB_ICE_HAS_ICECONNECTIONNUMBER)
      if(CMAKE_LIB_ICE_HAS_ICECONNECTIONNUMBER)
        SET (X11_X_PRE_LIBS ${X11_ICE_LIB})
        if(X11_SM_LIB)
          SET (X11_X_PRE_LIBS ${X11_SM_LIB} ${X11_X_PRE_LIBS})
        endif(X11_SM_LIB)
      endif(CMAKE_LIB_ICE_HAS_ICECONNECTIONNUMBER)
    endif(X11_ICE_FOUND)

    # Build the final list of libraries.
    set(X11_LIBRARIES ${X11_X_PRE_LIBS} ${X11_LIBRARIES} ${X11_X_EXTRA_LIBS})

    # Check whether we're pulling headers from multiple directories
    list(LENGTH X11_HDR_PATHS HDR_PATH_CNT)
    if("${HDR_PATH_CNT}" GREATER 1)
      message("\nNote: FindX11 is returning headers found in multiple paths.  The user may wish to verify that components are not being returned from multiple X11 installations.\n")
      if(CMAKE_SEARCH_OSX_PATHS)
	if(NOT "${CMAKE_SEARCH_OSX_PATHS}" STREQUAL "SYSTEM")
	  message("Note that CMAKE_SEARCH_OSX_PATHS is set to ${CMAKE_SEARCH_OSX_PATHS} - if ${CMAKE_SEARCH_OSX_PATHS} has an incomplete installation of X11, that may cause this issue - a possible workaround is to set CMAKE_SEARCH_OSX_PATHS to SYSTEM and not use ${CMAKE_SEARCH_OSX_PATHS}.\n")
	endif(NOT "${CMAKE_SEARCH_OSX_PATHS}" STREQUAL "SYSTEM")
      endif(CMAKE_SEARCH_OSX_PATHS)
      foreach(pathitem ${X11_HDR_PATHS})
	message("Headers found in ${pathitem}:")
	foreach(varitem ${X11_HDR_VARS})
	  get_filename_component(item_path "${${varitem}}" PATH)
	  if("${item_path}" STREQUAL "${pathitem}")
	    message("     ${varitem}:${${varitem}}")
	  endif("${item_path}" STREQUAL "${pathitem}")
	endforeach(varitem ${X11_HDR_PATHS})
	message(" ")
      endforeach(pathitem ${X11_HDR_PATHS})
    endif("${HDR_PATH_CNT}" GREATER 1)

    list(LENGTH X11_LIB_PATHS LIB_PATH_CNT)
    if("${LIB_PATH_CNT}" GREATER 1)
      message("\nNote: FindX11 is returning libraries found in multiple paths.  The user may wish to verify that components are not being returned from multiple X11 installations.\n")
      if(CMAKE_SEARCH_OSX_PATHS)
	if(NOT "${CMAKE_SEARCH_OSX_PATHS}" STREQUAL "SYSTEM")
	  message("Note that CMAKE_SEARCH_OSX_PATHS is set to ${CMAKE_SEARCH_OSX_PATHS} - if ${CMAKE_SEARCH_OSX_PATHS} has an incomplete installation of X11, that may cause this issue - a possible workaround is to set CMAKE_SEARCH_OSX_PATHS to SYSTEM and not use ${CMAKE_SEARCH_OSX_PATHS}.\n")
	endif(NOT "${CMAKE_SEARCH_OSX_PATHS}" STREQUAL "SYSTEM")
      endif(CMAKE_SEARCH_OSX_PATHS)

      foreach(pathitem ${X11_LIB_PATHS})
	message("Libraries found in ${pathitem}:")
	foreach(varitem ${X11_LIB_VARS})
	  get_filename_component(item_path "${${varitem}}" PATH)
	  if("${item_path}" STREQUAL "${pathitem}")
	    message("     ${varitem}:${${varitem}}")
	  endif("${item_path}" STREQUAL "${pathitem}")
	endforeach(varitem ${X11_LIB_PATHS})
	message(" ")
      endforeach(pathitem ${X11_LIB_PATHS})
    endif("${LIB_PATH_CNT}" GREATER 1)

    include(FindPackageMessage)
    FIND_PACKAGE_MESSAGE(X11 "Found X11: ${X11_X11_LIB}"
      "[${X11_X11_LIB}][${X11_INCLUDE_DIR}]")
  else(X11_FOUND)
    if(X11_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find X11")
    endif(X11_FIND_REQUIRED)
  endif(X11_FOUND)

  mark_as_advanced(
    X11_X11_INCLUDE_PATH
    X11_X11_LIB
    X11_Xext_LIB
    X11_Xau_LIB
    X11_Xau_INCLUDE_PATH
    X11_Xlib_INCLUDE_PATH
    X11_Xutil_INCLUDE_PATH
    X11_Xcomposite_INCLUDE_PATH
    X11_Xcomposite_LIB
    X11_Xaccess_INCLUDE_PATH
    X11_Xfixes_LIB
    X11_Xfixes_INCLUDE_PATH
    X11_Xrandr_LIB
    X11_Xrandr_INCLUDE_PATH
    X11_Xdamage_LIB
    X11_Xdamage_INCLUDE_PATH
    X11_Xrender_LIB
    X11_Xrender_INCLUDE_PATH
    X11_Xxf86misc_LIB
    X11_xf86misc_INCLUDE_PATH
    X11_xf86vmode_INCLUDE_PATH
    X11_Xi_LIB
    X11_Xi_INCLUDE_PATH
    X11_Xinerama_LIB
    X11_Xinerama_INCLUDE_PATH
    X11_XTest_LIB
    X11_XTest_INCLUDE_PATH
    X11_Xcursor_LIB
    X11_Xcursor_INCLUDE_PATH
    X11_dpms_INCLUDE_PATH
    X11_Xt_LIB
    X11_Xt_INCLUDE_PATH
    X11_Xdmcp_LIB
    X11_LIBRARIES
    X11_Xaccessrules_INCLUDE_PATH
    X11_Xaccessstr_INCLUDE_PATH
    X11_Xdmcp_INCLUDE_PATH
    X11_Xkb_INCLUDE_PATH
    X11_Xkblib_INCLUDE_PATH
    X11_Xscreensaver_INCLUDE_PATH
    X11_Xscreensaver_LIB
    X11_Xpm_INCLUDE_PATH
    X11_Xpm_LIB
    X11_Xinput_LIB
    X11_Xinput_INCLUDE_PATH
    X11_Xft_LIB
    X11_Xft_INCLUDE_PATH
    X11_Xshape_INCLUDE_PATH
    X11_Xv_LIB
    X11_Xv_INCLUDE_PATH
    X11_XShm_INCLUDE_PATH
    X11_ICE_LIB
    X11_ICE_INCLUDE_PATH
    X11_SM_LIB
    )
  set(CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_SAVE})
endif(UNIX)

# X11_FIND_REQUIRED_<component> could be checked too

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
