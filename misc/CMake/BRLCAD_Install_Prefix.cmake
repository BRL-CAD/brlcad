# The location in which to install BRL-CAD.  Only do this if
# CMAKE_INSTALL_PREFIX hasn't been set already.
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
      set(CMAKE_INSTALL_PREFIX "/usr/brlcad/dev-${BRLCAD_VERSION}")
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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
