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

if(CMAKE_INSTALL_PREFIX)
  if(NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
    get_filename_component(PATH_NORMALIZED "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}" ABSOLUTE)
    set(CMAKE_SYSTEM_IGNORE_PATH "${PATH_NORMALIZED}")
  endif(NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
endif(CMAKE_INSTALL_PREFIX)
if("${CMAKE_PROJECT_NAME}" STREQUAL "BRLCAD" AND BRLCAD_PREFIX)
  if(NOT "${BRLCAD_PREFIX}" STREQUAL "/usr" AND NOT "${BRLCAD_PREFIX}" STREQUAL "/usr/local")
    get_filename_component(PATH_NORMALIZED "${BRLCAD_PREFIX}/${LIB_DIR}" ABSOLUTE)
    set(CMAKE_SYSTEM_IGNORE_PATH "${PATH_NORMALIZED}")
  endif(NOT "${BRLCAD_PREFIX}" STREQUAL "/usr" AND NOT "${BRLCAD_PREFIX}" STREQUAL "/usr/local")
endif("${CMAKE_PROJECT_NAME}" STREQUAL "BRLCAD" AND BRLCAD_PREFIX)
mark_as_advanced(CMAKE_SYSTEM_IGNORE_PATH)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

