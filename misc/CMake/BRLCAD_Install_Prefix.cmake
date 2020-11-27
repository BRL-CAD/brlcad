# If we need it, use a standard string to stand in for build types.  This
# allows for easier replacing later in the logic
set(BUILD_TYPE_KEY "----BUILD_TYPE----")

#---------------------------------------------------------------------
# The location in which to install BRL-CAD.  Only do this if
# CMAKE_INSTALL_PREFIX hasn't been set already, to try and allow
# parent builds (if any) some control.

# TODO - generator expressions may be a way to replace some of this logic (or
# improve it) by letting us ditch the logic tracking whether the path is
# initialized to default - that's used to know whether or not it is safe to
# override CMAKE_INSTALL_PREFIX based on build type.  If we use the generator
# expressions, we won't have to be concerned about that - problem is that
# change will require updating all the install commands in the build target
# functions.  Third party logic will probably need to shift over to the new
# ExternalProject_Add approach first as well - that way unmodified install
# commands in 3rd party CMake files will be moot.
#
# if (NOT CMAKE_INSTALL_PREFIX)
# set(CMAKE_INSTALL_PREFIX "/usr/brlcad/")
#    if (NOT MSVC)
#       set(gen_exp $<IF:$<CONFIG:Debug>,dev-${BRLCAD_VERSION}/,$<IF:$<CONFIG:Release>,rel-${BRLCAD_VERSION}/${BRLCAD_VERSION},>>)
#    else (NOT MSVC)
#       set(gen_exp)
#        if(CMAKE_CL_64)
#          set(CMAKE_INSTALL_PREFIX "C:/Program Files/BRL-CAD ${BRLCAD_VERSION}")
#        else(CMAKE_CL_64)
#          set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/BRL-CAD ${BRLCAD_VERSION}")
#        endif(CMAKE_CL_64)
#    endif (NOT MSVC)
# endif (NOT CMAKE_INSTALL_PREFIX)
# install(TARGETS mged DESTINATION ${gen_exp}${BIN_DIR})
#
# Another (maybe better) option instead of generators might be to use the CONFIGURATIONS
# option in our macros and the ExternalProject_Add management:
# https://cmake.org/cmake/help/latest/command/install.html
#
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
	if(NOT CMAKE_CONFIGURATION_TYPES)
		if("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			set(CMAKE_INSTALL_PREFIX "/usr/brlcad/rel-${BRLCAD_VERSION}")
		else("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			set(CMAKE_INSTALL_PREFIX "/usr/brlcad/dev-${BRLCAD_VERSION}")
		endif("${CMAKE_BUILD_TYPE}" MATCHES "Release")
	else(NOT CMAKE_CONFIGURATION_TYPES)
		if(MSVC)
			if(CMAKE_CL_64)
				set(CMAKE_INSTALL_PREFIX "C:/Program Files/BRL-CAD ${BRLCAD_VERSION}")
			else(CMAKE_CL_64)
				set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/BRL-CAD ${BRLCAD_VERSION}")
			endif(CMAKE_CL_64)
		else(MSVC)
			set(CMAKE_INSTALL_PREFIX "/usr/brlcad/${BUILD_TYPE_KEY}-${BRLCAD_VERSION}")
		endif(MSVC)
	endif(NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "BRL-CAD install prefix" FORCE)
	set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
set(BRLCAD_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE STRING "BRL-CAD install prefix")
mark_as_advanced(BRLCAD_PREFIX)

# If we've a Release build with a Debug path or vice versa, warn about
# it.  A "make install" of a Release build into a dev install
# directory or vice versa is going to result in an install that
# doesn't respect BRL-CAD standard naming conventions.
if("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/dev-${BRLCAD_VERSION}")
  message(FATAL_ERROR "\nInstallation directory (CMAKE_INSTALL_PREFIX) is set to /usr/brlcad/dev-${BRLCAD_VERSION}, but build type is set to Release!\n")
endif("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/dev-${BRLCAD_VERSION}")
if("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/rel-${BRLCAD_VERSION}")
  message(FATAL_ERROR "\nInstallation directory (CMAKE_INSTALL_PREFIX) is set to /usr/brlcad/rel-${BRLCAD_VERSION}, but build type is set to Debug!\n")
endif("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/rel-${BRLCAD_VERSION}")

#------------------------------------------------------------------------------
# If CMAKE_INSTALL_PREFIX is "/usr", be VERY noisy about it - a make install in
# this location is dangerous/destructive on some systems.
find_program(SLEEP_EXEC sleep)
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT BRLCAD_ALLOW_INSTALL_TO_USR)
  message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nIt is STRONGLY recommended that you DO NOT install BRL-CAD into /usr as BRL-CAD provides several libraries that may conflict with other libraries (e.g. librt, libbu, libbn) on certain system configurations.\nSince our libraries predate all those that we're known to conflict with and are at the very core of our geometry services and project heritage, we have no plans to change the names of our libraries at this time.\nINSTALLING INTO /usr CAN MAKE A SYSTEM COMPLETELY UNUSABLE.  If you choose to continue installing into /usr, you do so entirely at your own risk.  You have been warned.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
  if(SLEEP_EXEC)
    execute_process(COMMAND ${SLEEP_EXEC} 15)
  endif(SLEEP_EXEC)
  message(FATAL_ERROR "If you wish to proceed using /usr as your prefix, define BRLCAD_ALLOW_INSTALL_TO_USR=1 for CMake")
endif("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT BRLCAD_ALLOW_INSTALL_TO_USR)

#---------------------------------------------------------------------
# Searching the system for packages presents something of a dilemma -
# in most situations it is Very Bad for a BRL-CAD build to be using
# older versions of libraries in install directories as search results.
# Generally, the desired behavior is to ignore whatever libraries are
# in the install directories, and only use external library results if
# they are something already found on the system due to non-BRL-CAD
# installation (source compile, package managers, etc.).  Unfortunately,
# CMake's standard behavior is to add CMAKE_INSTALL_PREFIX to the search
# path once defined, resulting in (for us) the unexpected behavior of
# returning old installed libraries when CMake is re-run in a directory.
#
# To work around this, there are two possible approaches.  One,
# identified by Maik Beckmann, operates on CMAKE_SYSTEM_PREFIX_PATH:
#
# http://www.cmake.org/pipermail/cmake/2010-October/040292.html
#
# The other, pointed out by Michael Hertling, uses the
# CMake_[SYSTEM_]IGNORE_PATH variables.
#
# http://www.cmake.org/pipermail/cmake/2011-May/044503.html
#
# BRL-CAD initially operated on CMAKE_SYSTEM_PREFIX_PATH, but has
# switched to using the *_IGNORE_PATH variables.  This requires
# CMake 2.8.3 or later.
#
# The complication with ignoring install paths is if we are
# installing to a "legitimate" system search path - i.e. our
# CMAKE_INSTALL_PREFIX value is standard enough that it is a legitimate
# search target for find_package. In this case, we can't exclude
# accidental hits on our libraries without also excluding legitimate
# find_package results.  So the net results are:
#
# 1.  If you are planning to install to a system directory (typically
#     a bad idea but the settings are legal) clean out the old system
#     first or accept that the old libraries will be found and used.
#
# 2.  For more custom paths, the logic below will avoid the value
#     of CMAKE_INSTALL_PREFIX in find_package searches
#
# (Note:  CMAKE_INSTALL_PREFIX must be checked in the case where someone
# sets it on the command line prior to CMake being run.  BRLCAD_PREFIX
# preserves the CMAKE_INSTALL_PREFIX setting from the previous CMake run.
# CMAKE_INSTALL_PREFIX does not seem to be immediately set in this context
# when CMake is re-run unless specified explicitly on the command line.
# To ensure the previous (and internally set) CMAKE_INSTALL_PREFIX value
# is available, BRLCAD_PREFIX is used to store the value in the cache.)

# TODO - CMAKE_IGNORE_PATH needs a list of explicit directories, not a
# list of prefixes - in other words, it ignores EXACTLY the specfied
# path and not any subdirectories under that path.  We need to use
# file(GLOB_RECURSE) to check CMAKE_INSTALL_PREFIX for any directories
# under it, once we've ruled out the standard system directories, and
# pass the full list in to CMAKE_IGNORE_PATH.  See:
# https://cmake.org/cmake/help/latest/variable/CMAKE_IGNORE_PATH.html

if(NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
	get_filename_component(PATH_NORMALIZED "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}" ABSOLUTE)
	if (CMAKE_CONFIGURATION_TYPES)
		foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
			if ("${cfg}" STREQUAL "Release")
				string(REPLACE "${BUILD_TYPE_KEY}" "rel" "${CMAKE_INSTALL_PREFIX}" RELPATH)
				set(CMAKE_SYSTEM_IGNORE_PATH "${CMAKE_SYSTEM_IGNORE_PATH};${RELPATH}")
				set(CMAKE_IGNORE_PATH "${CMAKE_SYSTEM_IGNORE_PATH};${RELPATH}")
			endif ("${cfg}" STREQUAL "Release")
			if ("${cfg}" STREQUAL "Debug")
				string(REPLACE "${BUILD_TYPE_KEY}" "dev" "${CMAKE_INSTALL_PREFIX}" DEVPATH)
				set(CMAKE_SYSTEM_IGNORE_PATH "${CMAKE_SYSTEM_IGNORE_PATH};${DEVPATH}")
				set(CMAKE_IGNORE_PATH "${CMAKE_SYSTEM_IGNORE_PATH};${DEVPATH}")
			endif ("${cfg}" STREQUAL "Debug")
			string(REPLACE "${BUILD_TYPE_KEY}" "${cfg}" "${CMAKE_INSTALL_PREFIX}" TYPEPATH)
			set(CMAKE_SYSTEM_IGNORE_PATH "${CMAKE_SYSTEM_IGNORE_PATH};${TYPEPATH}")
			set(CMAKE_IGNORE_PATH "${CMAKE_SYSTEM_IGNORE_PATH};${TYPEPATH}")
		endforeach(cfg ${CMAKE_CONFIGURATION_TYPES})
	else (CMAKE_CONFIGURATION_TYPES)
		set(CMAKE_SYSTEM_IGNORE_PATH "${PATH_NORMALIZED}")
		set(CMAKE_IGNORE_PATH "${PATH_NORMALIZED}")
	endif (CMAKE_CONFIGURATION_TYPES)
endif(NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")

#------------------------------------------------------------------------------
# Now that we know the install prefix, generate the binary for calculating
# and reporting time deltas
generate_dreport()

