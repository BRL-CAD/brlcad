#             F I N K _ M A C P O R T S . C M A K E
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
# Fink and/or MacPorts complicate searching for libraries
# on OSX.  Provide a way to specify whether to search using
# them (if available) or just use System paths.
#
# CMAKE_SEARCH_OSX_PATHS is used to specify the primary source for
# libraries and headers when the situation might be confused - the
# setting may be one of:
#
# SYSTEM - Use the system libraries in preference to Fink or MacPorts
# FINK - Prefer the Fink libraries
# MACPORTS - Prefer the MacPorts libraries
#
# Library and header path variables are defined to allow for more
# controlled searching:
#
# CMAKE_FINK_LIBRARY_PATH
# CMAKE_FINK_INCLUDE_PATH
# CMAKE_MACPORTS_LIBRARY_PATH
# CMAKE_MACPORTS_INCLUDE_PATH

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  # first, look for the main Fink and MacPorts programs.
  find_program(PORT_EXEC port)
  mark_as_advanced(PORT_EXEC)
  find_program(FINK_EXEC fink)
  mark_as_advanced(FINK_EXEC)

  # If either or both are installed, we need to address the issues.  Otherwise,
  # this file is a no-op.
  if(PORT_EXEC OR FINK_EXEC)
    if(CMAKE_SEARCH_OSX_PATHS)
      string(TOUPPER "${CMAKE_SEARCH_OSX_PATHS}" paths_upper)
      set(CMAKE_SEARCH_OSX_PATHS ${paths_upper})
      if(NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")
	message(WARNING "Unknown value ${CMAKE_SEARCH_OSX_PATHS} supplied for CMAKE_SEARCH_OSX_PATHS - defaulting to SYSTEM")
	set(CMAKE_SEARCH_OSX_PATHS "SYSTEM")
      endif(NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")
    endif(CMAKE_SEARCH_OSX_PATHS)

    # List out the viable options for this platform
    set(OSX_PATH_OPTIONS "SYSTEM")
    if(PORT_EXEC)
      set(OSX_PATH_OPTIONS ${OSX_PATH_OPTIONS} "MACPORTS")
    endif(PORT_EXEC)
    if(FINK_EXEC)
      set(OSX_PATH_OPTIONS ${OSX_PATH_OPTIONS} "FINK")
    endif(FINK_EXEC)

    # If we don't already have a value set, set one.
    if(NOT CMAKE_SEARCH_OSX_PATHS)
      if(PORT_EXEC AND NOT FINK_EXEC)
	set(CMAKE_SEARCH_OSX_PATHS "MACPORTS" CACHE STRING "Use MacPorts")
      endif(PORT_EXEC AND NOT FINK_EXEC)
      if(FINK_EXEC AND NOT PORT_EXEC)
	set(CMAKE_SEARCH_OSX_PATHS "FINK" CACHE STRING "Use Fink")
      endif(FINK_EXEC AND NOT PORT_EXEC)
      if(NOT CMAKE_SEARCH_OSX_PATHS)
	set(CMAKE_SEARCH_OSX_PATHS "SYSTEM" CACHE STRING "Use System")
      endif(NOT CMAKE_SEARCH_OSX_PATHS)
    endif(NOT CMAKE_SEARCH_OSX_PATHS)

    # Set our properties for CMake-GUI
    set_property(CACHE CMAKE_SEARCH_OSX_PATHS PROPERTY STRINGS ${OSX_PATH_OPTIONS})

    # If the user has picked a setting that is unsupported by the system, warn
    # them and fix it...
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT PORT_EXEC)
      message(WARNING "CMAKE_SEARCH_OSX_PATHS set to MACPORTS, but port executable not found - defaulting to SYSTEM")
      set(CMAKE_SEARCH_OSX_PATHS "SYSTEM")
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT PORT_EXEC)
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK" AND NOT FINK_EXEC)
      message(WARNING "CMAKE_SEARCH_OSX_PATHS set to FINK, but fink executable not found - defaulting to SYSTEM")
      set(CMAKE_SEARCH_OSX_PATHS "SYSTEM")
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK" AND NOT FINK_EXEC)

    # Deduce the MacPorts paths from PORT_EXEC
    IF(PORT_EXEC)
      get_filename_component(port_binpath ${PORT_EXEC} PATH)
      get_filename_component(port_path ${port_binpath} PATH)
      get_filename_component(port_path_normalized ${port_path} ABSOLUTE)
      SET(CMAKE_MACPORTS_LIBRARY_PATH ${port_path_normalized}/lib CACHE STRING "MacPorts library path" FORCE)
      SET(CMAKE_MACPORTS_INCLUDE_PATH ${port_path_normalized}/include CACHE STRING "MacPorts include path" FORCE)
      if(CMAKE_SYSTEM_IGNORE_PATH)
	list(REMOVE_ITEM CMAKE_SYSTEM_IGNORE_PATH "${port_path_normalized}/lib")
      endif(CMAKE_SYSTEM_IGNORE_PATH)
    ENDIF(PORT_EXEC)

    # Deduce the Fink paths from FINK_EXEC
    if(FINK_EXEC)
      get_filename_component(fink_binpath ${FINK_EXEC} PATH)
      get_filename_component(fink_path ${fink_binpath} PATH)
      get_filename_component(fink_path_normalized ${fink_path} ABSOLUTE)
      SET(CMAKE_FINK_LIBRARY_PATH ${fink_path_normalized}/lib CACHE STRING "Fink library path" FORCE)
      SET(CMAKE_FINK_INCLUDE_PATH ${fink_path_normalized}/include CACHE STRING "Fink include path" FORCE)
      if(CMAKE_SYSTEM_IGNORE_PATH)
	list(REMOVE_ITEM CMAKE_SYSTEM_IGNORE_PATH "${CMAKE_FINK_LIBRARY_PATH}")
      endif(CMAKE_SYSTEM_IGNORE_PATH)
    endif(FINK_EXEC)

    # If we're using MACPORTS, set some variables accordingly
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS")
      set(CMAKE_LIBRARY_PATH ${CMAKE_MACPORTS_LIBRARY_PATH} ${CMAKE_LIBRARY_PATH})
      set(CMAKE_INCLUDE_PATH ${CMAKE_MACPORTS_INCLUDE_PATH} ${CMAKE_INCLUDE_PATH})
      # If Fink is lurking, we need to avoid it - we don't want to mix MacPorts and Fink
      if("${CMAKE_FINK_LIBRARY_PATH}" STREQUAL "${CMAKE_MACPORTS_LIBRARY_PATH}")
	# We CAN'T ignore Fink - it looks like it's installed in the same place as MacPorts. Not Good.
	message(WARNING "Both Fink and MacPorts appear to be installed in ${fink_path_normalized}, search results unpredictable")
      else("${CMAKE_FINK_LIBRARY_PATH}" STREQUAL "${CMAKE_MACPORTS_LIBRARY_PATH}")
	# Ignore Fink
	set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${CMAKE_FINK_LIBRARY_PATH})
	if(CMAKE_LIBRARY_PATH)
	  list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${CMAKE_FINK_LIBRARY_PATH}")
	endif(CMAKE_LIBRARY_PATH)
	if(CMAKE_INCLUDE_PATH)
	  list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${CMAKE_FINK_INCLUDE_PATH}")
	endif(CMAKE_INCLUDE_PATH)
      endif("${CMAKE_FINK_LIBRARY_PATH}" STREQUAL "${CMAKE_MACPORTS_LIBRARY_PATH}")
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS")

    # If we're using FINK, set some variables accordingly
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")
      set(CMAKE_LIBRARY_PATH ${CMAKE_FINK_LIBRARY_PATH} ${CMAKE_LIBRARY_PATH})
      set(CMAKE_INCLUDE_PATH ${CMAKE_FINK_INCLUDE_PATH} ${CMAKE_INCLUDE_PATH})
      # If MacPorts is lurking, we need to avoid it - we don't want to mix MacPorts and Fink
      if("${CMAKE_FINK_LIBRARY_PATH}" STREQUAL "${CMAKE_MACPORTS_LIBRARY_PATH}")
	message(WARNING "Both Fink and MacPorts appear to be installed in ${fink_path_normalized}, search results unpredictable")
      else("${CMAKE_FINK_LIBRARY_PATH}" STREQUAL "${CMAKE_MACPORTS_LIBRARY_PATH}")
	set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${CMAKE_MACPORTS_LIBRARY_PATH})
	if(CMAKE_LIBRARY_PATH)
	  list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${CMAKE_MACPORTS_LIBRARY_PATH}")
	endif(CMAKE_LIBRARY_PATH)
	if(CMAKE_INCLUDE_PATH)
	  list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${CMAKE_MACPORTS_INCLUDE_PATH}")
	endif(CMAKE_INCLUDE_PATH)
      endif("${CMAKE_FINK_LIBRARY_PATH}" STREQUAL "${CMAKE_MACPORTS_LIBRARY_PATH}")
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")

    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM")
      set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${CMAKE_FINK_LIBRARY_PATH} ${CMAKE_FINK_INCLUDE_PATH})
      set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${CMAKE_MACPORTS_LIBRARY_PATH} ${CMAKE_MACPORTS_INCLUDE_PATH})
      if(CMAKE_LIBRARY_PATH)
	list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${CMAKE_FINK_LIBRARY_PATH}")
	list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${CMAKE_MACPORTS_LIBRARY_PATH}")
      endif(CMAKE_LIBRARY_PATH)
      if(CMAKE_INCLUDE_PATH)
	list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${CMAKE_FINK_INCLUDE_PATH}")
	list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${CMAKE_MACPORTS_INCLUDE_PATH}")
      endif(CMAKE_INCLUDE_PATH)
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM")
  endif(PORT_EXEC OR FINK_EXEC)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
