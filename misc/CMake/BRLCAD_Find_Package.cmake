#       B R L C A D _ F I N D _ P A C K A G E . C M A K E
#
# BRL-CAD
#
# Copyright (c) 2020-2025 United States Government as represented by
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

# By default, find_package stores values in the Cache to avoid having
# to search for the same thing multiple times.  Normally this is
# useful to make repeated configure passes faster, but in BRL-CAD's
# case we want to check system vs. local every time in case there has
# been a change (i.e. a user has added a bundled copy of a library
# that was previously being found on the system.)
#
# To do this generically, without having to be aware of what values
# each find_package call is setting, we utilize the CACHE_VARIABLES
# directory property to do a before and after detection of what
# was changed.
#
# Note that this must be a macro, since we do want find_package's
# results to be set in scope the same way they would be for a raw
# find_package call.

macro(BRLCAD_Find_Package pkg_name)
  cmake_parse_arguments(F "REQUIRED;NOINSTALL" "SYSPATTERN" "" ${ARGN})

  # If we have a bundled copy of this package, always use that.  Packages we
  # are not going to install are not copied to CMAKE_BINARY_DIR, so set the
  # *_ROOT variable accordingly
  string(TOUPPER ${pkg_name} pkg_upper)
  if(F_NOINSTALL)
    set(${pkg_name}_ROOT "${BRLCAD_EXT_NOINSTALL_DIR}")
    set(${pkg_upper}_ROOT "${BRLCAD_EXT_NOINSTALL_DIR}")
  else(F_NOINSTALL)
    set(${pkg_name}_ROOT "${CMAKE_BINARY_DIR}")
    set(${pkg_upper}_ROOT "${CMAKE_BINARY_DIR}")
  endif(F_NOINSTALL)

  # Clear the old status value, if any
  unset(${pkg_upper}_STATUS CACHE)

  # Store the state of the cache variables before find_package - see
  # https://discourse.cmake.org/t/cmake-properties-for-options/250/2
  get_property(cv_ctrl DIRECTORY PROPERTY CACHE_VARIABLES)

  # Generally speaking we're not looking to use frameworks in BRL-CAD - try
  # them only at the end, if we don't have standard libraries/headers.
  set(CMAKE_FIND_FRAMEWORK LAST)

  # Execute the actual find_package call
  if(F_REQUIRED)
    find_package(${pkg_name} REQUIRED)
  else(F_REQUIRED)
    find_package(${pkg_name})
  endif(F_REQUIRED)

  # Read the cache variables again after find_package is finished
  get_property(cv_new DIRECTORY PROPERTY CACHE_VARIABLES)

  # Using the list of variables that were already set before the find_package
  # call, trim the new list down to just the newly set variables.  This is
  # one possible source of paths, but we also need this to clear the cache
  # versions of the results.  We want find_package to re-run on each configure
  # as a clean test of the current state of the system, and to do that we only
  # allow the values from a find_package to persist in the current working
  # CMake state.  To prevent their being cached (the default CMake behavior)
  # we need to manually intervene and clear them.
  list(REMOVE_ITEM cv_new ${cv_ctrl})

  # If find_package is using an imported target, get the location from
  # the target.
  set(brlcad_find_package_tloc)
  if (TARGET ${pkg_name}::${pkg_name})
    # TODO - we may need to check CONFIG versions of this property as well.
    get_target_property(brlcad_find_package_tloc ${pkg_name}::${pkg_name} LOCATION)
    if (NOT brlcad_find_package_tloc)
      get_target_property(llibs ${pkg_name}::${pkg_name} INTERFACE_LINK_LIBRARIES)
      foreach(ll ${llibs})
	if ("${ll}" MATCHES "${pkg_name}::.*")
	  get_target_property(brlcad_find_package_tloc ${ll} LOCATION)
	endif ("${ll}" MATCHES "${pkg_name}::.*")
      endforeach(ll ${llibs})
    endif (NOT brlcad_find_package_tloc)
    list(APPEND cv_new brlcad_find_package_tloc)
  endif (TARGET ${pkg_name}::${pkg_name})

  if (BRLCAD_FIND_DEBUG_MODE)
    message("new cache items: ${cv_new}")
  endif (BRLCAD_FIND_DEBUG_MODE)

  set(is_bundled FALSE)
  foreach(nc ${cv_new})
    if (BRLCAD_FIND_DEBUG_MODE)
      message("Checking ${nc}: ${${nc}}")
    endif (BRLCAD_FIND_DEBUG_MODE)

    # We don't know what vars were set, but check each one that was set
    # for a local path.  If we find one, we have a bundled package
    is_subpath("${CMAKE_BINARY_DIR}" "${${nc}}" LOCAL_TEST)
    if(LOCAL_TEST)
      if (BRLCAD_FIND_DEBUG_MODE)
	message("  - Local path found")
      endif (BRLCAD_FIND_DEBUG_MODE)
      set(is_bundled TRUE)
    endif(LOCAL_TEST)

    #message("find_package cache var (${nc}): ${${nc}}")

    # Unset will wipe out both the cache and the working variable
    # value - we want to ONLY eliminate it from the cache, so stash
    # the value.
    set(${nc}_tmp ${${nc}})
    # Clear the cache entry
    unset(${nc} CACHE)
    # Restore the working value with a non-cache set.
    set(${nc} ${${nc}_tmp})
  endforeach(nc ${cv_new})

  # If we need to include header files associated with this package using
  # the SYSTEM directive, user should have supplied a pattern to match
  # with the SYSPATTERN flag
  if(F_SYSPATTERN)
    set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} ${F_SYSPATTERN})
    list(REMOVE_DUPLICATES SYS_INCLUDE_PATTERNS)
    set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} CACHE STRING "Bundled system include dirs" FORCE)
  endif(F_SYSPATTERN)

  # Since we're not letting find_package write to the cache, set a status
  # variable that the BRL-CAD summary printing can use to report the result
  if(is_bundled)
    set(${pkg_upper}_STATUS "Bundled" CACHE STRING "${pkg_name} bundled status")
  else(is_bundled)
    if(${pkg_name}_FOUND OR ${pkg_upper}_FOUND)
      set(${pkg_upper}_STATUS "System" CACHE STRING "${pkg_name} bundled status")
    else()
      set(${pkg_upper}_STATUS "NotFound" CACHE STRING "${pkg_name} bundled status")
    endif()
  endif(is_bundled)
  if (BRLCAD_FIND_DEBUG_MODE)
    message("${pkg_upper}_STATUS: ${${pkg_upper}_STATUS}")
  endif (BRLCAD_FIND_DEBUG_MODE)
endmacro()

# Local Variables:
# mode: cmake
# tab-width: 2
# indent-tabs-mode: nil
# End:
# ex: shiftwidth=2 tabstop=8
