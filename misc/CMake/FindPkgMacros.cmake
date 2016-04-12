#             F I N D P K G M A C R O S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2016 United States Government as represented by
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
#-------------------------------------------------------------------
# This file is part of the CMake build system for OGRE
#     (Object-oriented Graphics Rendering Engine)
# For the latest info, see http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

##################################################################
# Provides some common functionality for the FindPackage modules
##################################################################

# Begin processing of package
macro(findpkg_begin PREFIX)
  if (NOT ${PREFIX}_FIND_QUIETLY)
    message(STATUS "Looking for ${PREFIX}...")
  endif ()
endmacro(findpkg_begin)

# Display a status message unless FIND_QUIETLY is set
macro(pkg_message PREFIX)
  if (NOT ${PREFIX}_FIND_QUIETLY)
    message(STATUS ${ARGN})
  endif ()
endmacro(pkg_message)

# Get environment variable, define it as ENV_$var and make sure backslashes are converted to forward slashes
macro(getenv_path VAR)
  set(ENV_${VAR} $ENV{${VAR}})
  # replace won't work if var is blank
  if (ENV_${VAR})
    string( REGEX REPLACE "\\\\" "/" ENV_${VAR} ${ENV_${VAR}} )
  endif ()
endmacro(getenv_path)

# Construct search paths for includes and libraries from a PREFIX_PATH
macro(create_search_paths PREFIX)
  foreach(dir ${${PREFIX}_PREFIX_PATH})
    set(${PREFIX}_INC_SEARCH_PATH ${${PREFIX}_INC_SEARCH_PATH}
      ${dir}/include ${dir}/Include ${dir}/include/${PREFIX} ${dir}/Headers)
    set(${PREFIX}_LIB_SEARCH_PATH ${${PREFIX}_LIB_SEARCH_PATH}
      ${dir}/lib ${dir}/Lib ${dir}/lib/${PREFIX} ${dir}/Libs)
    set(${PREFIX}_BIN_SEARCH_PATH ${${PREFIX}_BIN_SEARCH_PATH}
      ${dir}/bin)
  endforeach(dir)
  set(${PREFIX}_FRAMEWORK_SEARCH_PATH ${${PREFIX}_PREFIX_PATH})
endmacro(create_search_paths)

# clear cache variables if a certain variable changed
macro(clear_if_changed TESTVAR)
  # test against internal check variable
  # HACK: Apparently, adding a variable to the cache cleans up the list
  # a bit. We need to also remove any empty strings from the list, but
  # at the same time ensure that we are actually dealing with a list.
  list(APPEND ${TESTVAR} "")
  list(REMOVE_ITEM ${TESTVAR} "")
  if (NOT "${${TESTVAR}}" STREQUAL "${${TESTVAR}_INT_CHECK}")
    message(STATUS "${TESTVAR} changed.")
    foreach(var ${ARGN})
      set(${var} "NOTFOUND" CACHE STRING "x" FORCE)
    endforeach(var)
  endif ()
  set(${TESTVAR}_INT_CHECK ${${TESTVAR}} CACHE INTERNAL "x" FORCE)
endmacro(clear_if_changed)

# Try to get some hints from pkg-config, if available
macro(use_pkgconfig PREFIX PKGNAME)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(${PREFIX} ${PKGNAME})
  endif ()
endmacro (use_pkgconfig)

# Couple a set of release AND debug libraries (or frameworks)
macro(make_library_set PREFIX)
  if (${PREFIX}_FWK)
    set(${PREFIX} ${${PREFIX}_FWK})
  elseif (${PREFIX}_REL AND ${PREFIX}_DBG)
    set(${PREFIX} optimized ${${PREFIX}_REL} debug ${${PREFIX}_DBG})
  elseif (${PREFIX}_REL)
    set(${PREFIX} ${${PREFIX}_REL})
  elseif (${PREFIX}_DBG)
    set(${PREFIX} ${${PREFIX}_DBG})
  endif ()
endmacro(make_library_set)

# Generate debug names from given release names
macro(get_debug_names PREFIX)
  foreach(i ${${PREFIX}})
    set(${PREFIX}_DBG ${${PREFIX}_DBG} ${i}d ${i}D ${i}_d ${i}_D ${i}_debug ${i})
  endforeach(i)
endmacro(get_debug_names)

# Add the parent dir from DIR to VAR
macro(add_parent_dir VAR DIR)
  get_filename_component(${DIR}_TEMP "${${DIR}}/.." ABSOLUTE)
  set(${VAR} ${${VAR}} ${${DIR}_TEMP})
endmacro(add_parent_dir)

# Do the final processing for the package find.
macro(findpkg_finish PREFIX)
  # skip if already processed during this run
  if (NOT ${PREFIX}_FOUND)
    if (${PREFIX}_INCLUDE_DIR AND ${PREFIX}_LIBRARY)
      set(${PREFIX}_FOUND TRUE CACHE BOOL "Found ${PREFIX}")
      set(${PREFIX}_INCLUDE_DIRS ${${PREFIX}_INCLUDE_DIR} CACHE STRING "${PREFIX} include dirs")
      set(${PREFIX}_LIBRARIES ${${PREFIX}_LIBRARY} CACHE STRING "${PREFIX} libraries")
      if (NOT ${PREFIX}_FIND_QUIETLY)
        message(STATUS "Found ${PREFIX}: ${${PREFIX}_LIBRARIES}")
	set(${PREFIX}_FIND_QUIETLY 1 CACHE BOOL "find quietly")
      endif ()
    else ()
      if (NOT ${PREFIX}_FIND_QUIETLY)
        message(STATUS "Could not locate ${PREFIX}")
      endif ()
      if (${PREFIX}_FIND_REQUIRED)
        message(FATAL_ERROR "Required library ${PREFIX} not found! Install the library (including dev packages) and try again. If the library is already installed, set the missing variables manually in cmake.")
      endif ()
    endif ()

    mark_as_advanced(${PREFIX}_INCLUDE_DIR ${PREFIX}_LIBRARY ${PREFIX}_LIBRARY_REL ${PREFIX}_LIBRARY_DBG ${PREFIX}_LIBRARY_FWK)
  endif ()
endmacro(findpkg_finish)


# Slightly customised framework finder
macro(findpkg_framework fwk)
  if(APPLE)
    set(${fwk}_FRAMEWORK_PATH
      ${${fwk}_FRAMEWORK_SEARCH_PATH}
      ${CMAKE_FRAMEWORK_PATH}
      ~/Library/Frameworks
      /Library/Frameworks
      /System/Library/Frameworks
      /Network/Library/Frameworks
      /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.0.sdk/System/Library/Frameworks/
      "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release"
      "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug"
      ${OGRE_PREFIX_PATH}/lib/Release
      ${OGRE_PREFIX_PATH}/lib/Debug
      ${OGRE_PREFIX_BUILD}/lib/Release
      ${OGRE_PREFIX_BUILD}/lib/Debug
      )
    foreach(dir ${${fwk}_FRAMEWORK_PATH})
      set(fwkpath ${dir}/${fwk}.framework)
      if(EXISTS ${fwkpath})
        set(${fwk}_FRAMEWORK_INCLUDES ${${fwk}_FRAMEWORK_INCLUDES}
          ${fwkpath}/Headers ${fwkpath}/PrivateHeaders)
        set(${fwk}_FRAMEWORK_PATH ${dir})
        if (NOT ${fwk}_LIBRARY_FWK)
          set(${fwk}_LIBRARY_FWK "-framework ${fwk}")
        endif ()
      endif(EXISTS ${fwkpath})
    endforeach(dir)
  endif(APPLE)
endmacro(findpkg_framework)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
