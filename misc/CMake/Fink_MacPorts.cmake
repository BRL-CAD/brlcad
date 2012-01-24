#             F I N K _ M A C P O R T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
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
# Fink and/or Macports complicate searching for libraries
# on OSX.  Provide a way to specify whether to search using
# them (if available) or just use System paths.

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  find_program(PORT_EXEC port)
  mark_as_advanced(PORT_EXEC)
  find_program(FINK_EXEC fink)
  mark_as_advanced(FINK_EXEC)
  if(PORT_EXEC OR FINK_EXEC)
    if(NOT CMAKE_SEARCH_OSX_PATHS)
      set(OSX_PATH_OPTIONS "System")
      if(PORT_EXEC)
	list(APPEND OSX_PATH_OPTIONS "MacPorts")
	if(NOT FINK_EXEC)
	  set(CMAKE_SEARCH_OSX_PATHS "MacPorts" CACHE STRING "Use Macports")
	endif(NOT FINK_EXEC)
      endif(PORT_EXEC)
      if(FINK_EXEC)
	list(APPEND OSX_PATH_OPTIONS "Fink")
	if(NOT PORT_EXEC)
	  set(CMAKE_SEARCH_OSX_PATHS "Fink" CACHE STRING "Use Fink")
	endif(NOT PORT_EXEC)
      endif(FINK_EXEC)
      if(NOT CMAKE_SEARCH_OSX_PATHS)
	set(CMAKE_SEARCH_OSX_PATHS "System" CACHE STRING "Use System")
      endif(NOT CMAKE_SEARCH_OSX_PATHS)
      set_property(CACHE CMAKE_SEARCH_OSX_PATHS PROPERTY STRINGS ${OSX_PATH_OPTIONS})
    endif(NOT CMAKE_SEARCH_OSX_PATHS)
    string(TOUPPER "${CMAKE_SEARCH_OSX_PATHS}" paths_upper)
    set(CMAKE_SEARCH_OSX_PATHS ${paths_upper})
    if(NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")
      message(WARNING "Unknown value ${CMAKE_SEARCH_OSX_PATHS} supplied for CMAKE_SEARCH_OSX_PATHS - defaulting to System")
      set(CMAKE_SEARCH_OSX_PATHS "SYSTEM")
    endif(NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT ${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT PORT_EXEC) 
      message(WARNING "CMAKE_SEARCH_OSX_PATHS set to MacPorts, but port executable not found - defaulting to System")
      set(CMAKE_SEARCH_OSX_PATHS "SYSTEM")
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS" AND NOT PORT_EXEC) 
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK" AND NOT FINK_EXEC) 
      message(WARNING "CMAKE_SEARCH_OSX_PATHS set to Fink, but fink executable not found - defaulting to System")
      set(CMAKE_SEARCH_OSX_PATHS "SYSTEM")
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK" AND NOT FINK_EXEC)
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS")
      get_filename_component(port_binpath ${PORT_EXEC} PATH)
      get_filename_component(port_path ${port_binpath} PATH)
      get_filename_component(port_path_normalized ${port_path} ABSOLUTE)
      set(CMAKE_LIBRARY_PATH ${port_path_normalized}/lib ${CMAKE_LIBRARY_PATH})
      set(CMAKE_INCLUDE_PATH ${port_path_normalized}/include ${CMAKE_INCLUDE_PATH})
      if(CMAKE_SYSTEM_IGNORE_PATH)
	list(REMOVE_ITEM CMAKE_SYSTEM_IGNORE_PATH "${port_path_normalized}/lib")
      endif(CMAKE_SYSTEM_IGNORE_PATH)
      if(FINK_EXEC)
	get_filename_component(fink_binpath ${FINK_EXEC} PATH)
	get_filename_component(fink_path ${fink_binpath} PATH)
	get_filename_component(fink_path_normalized ${fink_path} ABSOLUTE)
	if("${fink_path_normalized}" STREQUAL "${port_path_normalized}")
	  message(WARNING "Both Fink and MacPorts appear to be installed in ${fink_path_normalized}, search results unpredictable")
	else("${fink_path_normalized}" STREQUAL "${port_path_normalized}")
	  set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${fink_path_normalized}/lib)
	  if(CMAKE_LIBRARY_PATH)
	    list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${fink_path_normalized}/lib")
	  endif(CMAKE_LIBRARY_PATH)
	  if(CMAKE_INCLUDE_PATH)
	    list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${fink_path_normalized}/include")
	  endif(CMAKE_INCLUDE_PATH)
	endif("${fink_path_normalized}" STREQUAL "${port_path_normalized}")
      endif(FINK_EXEC)
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "MACPORTS")
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")
      get_filename_component(fink_binpath ${FINK_EXEC} PATH)
      get_filename_component(fink_path ${fink_binpath} PATH)
      get_filename_component(fink_path_normalized ${fink_path} ABSOLUTE)
      set(CMAKE_LIBRARY_PATH ${fink_path_normalized}/lib ${CMAKE_LIBRARY_PATH})
      set(CMAKE_INCLUDE_PATH ${fink_path_normalized}/include ${CMAKE_INCLUDE_PATH})
      if(CMAKE_SYSTEM_IGNORE_PATH)
	list(REMOVE_ITEM CMAKE_SYSTEM_IGNORE_PATH "${fink_path_normalized}/lib")
      endif(CMAKE_SYSTEM_IGNORE_PATH)
      if(PORT_EXEC)
	get_filename_component(port_binpath ${PORT_EXEC} PATH)
	get_filename_component(port_path ${port_binpath} PATH)
	get_filename_component(port_path_normalized ${port_path} ABSOLUTE)
	if("${fink_path_normalized}" STREQUAL "${port_path_normalized}")
	  message(WARNING "Both Fink and MacPorts appear to be installed in ${fink_path_normalized}, search results unpredictable")
	else("${fink_path_normalized}" STREQUAL "${port_path_normalized}")
	  set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${port_path_normalized}/lib)
	  if(CMAKE_LIBRARY_PATH)
	    list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${port_path_normalized}/lib")
	  endif(CMAKE_LIBRARY_PATH)
	  if(CMAKE_INCLUDE_PATH)
	    list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${port_path_normalized}/include")
	  endif(CMAKE_INCLUDE_PATH)
	endif("${fink_path_normalized}" STREQUAL "${port_path_normalized}")
      endif(PORT_EXEC)
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "FINK")
    if(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM")
      if(FINK_EXEC)
	get_filename_component(fink_binpath ${FINK_EXEC} PATH)
	get_filename_component(fink_path ${fink_binpath} PATH)
	get_filename_component(fink_path_normalized ${fink_path} ABSOLUTE)
	set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${fink_path_normalized}/lib)
	set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${fink_path_normalized}/include)
	if(CMAKE_LIBRARY_PATH)
	  list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${fink_path_normalized}/lib")
	endif(CMAKE_LIBRARY_PATH)
	if(CMAKE_INCLUDE_PATH)
	  list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${fink_path_normalized}/include")
	endif(CMAKE_INCLUDE_PATH)
      endif(FINK_EXEC)
      if(PORT_EXEC)
	get_filename_component(port_binpath ${PORT_EXEC} PATH)
	get_filename_component(port_path ${port_binpath} PATH)
	get_filename_component(port_path_normalized ${port_path} ABSOLUTE)
	set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${port_path_normalized}/lib)
	set(CMAKE_SYSTEM_IGNORE_PATH ${CMAKE_SYSTEM_IGNORE_PATH} ${port_path_normalized}/include)
	if(CMAKE_LIBRARY_PATH)
	  list(REMOVE_ITEM CMAKE_LIBRARY_PATH "${port_path_normalized}/lib")
	endif(CMAKE_LIBRARY_PATH)
	if(CMAKE_INCLUDE_PATH)
	  list(REMOVE_ITEM CMAKE_INCLUDE_PATH "${port_path_normalized}/include")
	endif(CMAKE_INCLUDE_PATH)
      endif(PORT_EXEC)
    endif(${CMAKE_SEARCH_OSX_PATHS} STREQUAL "SYSTEM")
  endif(PORT_EXEC OR FINK_EXEC)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
