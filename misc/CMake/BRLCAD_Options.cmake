#             B R L C A D _ O P T I O N S . C M A K E
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
#-----------------------------------------------------------------------------
# Build Type aware option
macro(AUTO_OPTION username varname debug_state release_state)
  string(LENGTH "${${username}}" ${username}_SET)

  if(NOT ${username}_SET)
    set(${username} "AUTO" CACHE STRING "auto option" FORCE)
  endif(NOT ${username}_SET)

  set_property(CACHE ${username} PROPERTY STRINGS AUTO "ON" "OFF")

  string(TOUPPER "${${username}}" username_upper)
  set(${username} ${username_upper})

  if(NOT ${${username}} MATCHES "AUTO" AND NOT ${${username}} MATCHES "ON" AND NOT ${${username}} MATCHES "OFF")
    message(WARNING "Unknown value ${${username}} supplied for ${username} - defaulting to AUTO.\nValid options are AUTO, ON and OFF")
    set(${username} "AUTO" CACHE STRING "auto option" FORCE)
  endif(NOT ${${username}} MATCHES "AUTO" AND NOT ${${username}} MATCHES "ON" AND NOT ${${username}} MATCHES "OFF")

  # If the "parent" setting isn't AUTO, do what it says
  if(NOT ${${username}} MATCHES "AUTO")
    set(${varname} ${${username}})
  endif(NOT ${${username}} MATCHES "AUTO")

  # If we don't understand the build type and have an AUTO setting,
  # varname is not set.
  if(CMAKE_BUILD_TYPE AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    if(NOT ${${username}} MATCHES "AUTO")
      set(${varname} ${${username}})
    endif(NOT ${${username}} MATCHES "AUTO")
  endif(CMAKE_BUILD_TYPE AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")

  # If we DO understand the build type and have AUTO, be smart
  if("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${${username}} MATCHES "AUTO")
    set(${varname} ${release_state})
    set(${username} "${release_state} (AUTO)" CACHE STRING "auto option" FORCE)
  endif("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${${username}} MATCHES "AUTO")

  if("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${${username}} MATCHES "AUTO")
    set(${varname} ${debug_state})
    set(${username} "${debug_state} (AUTO)" CACHE STRING "auto option" FORCE)
  endif("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${${username}} MATCHES "AUTO")

  if(NOT CMAKE_BUILD_TYPE AND ${${username}} MATCHES "AUTO")
    set(${varname} ${debug_state})
    set(${username} "${debug_state} (AUTO)" CACHE STRING "auto option" FORCE)
  endif(NOT CMAKE_BUILD_TYPE AND ${${username}} MATCHES "AUTO")

endmacro(AUTO_OPTION varname release_state debug_state)

#-----------------------------------------------------------------------------
# For "top-level" BRL-CAD options, some extra work is in order - descriptions and
# lists of aliases are supplied, and those are automatically addressed by this
# macro.  In this context, "aliases" are variables which may be defined on the
# CMake command line that are intended to set the value of th BRL-CAD option in
# question, but use some other name.  Aliases may be common typos, different
# nomenclature, or anything that the developer things should lead to a given
# option being set.  The documentation is auto-formatted into a text document
# describing all BRL-CAD options.
#
# We also generate the "translation lines" for converting between Autotools'
# configure style option syntax and CMake's variables, and append that to
# the generated configure.new file.

# Handle aliases for BRL-CAD options
macro(OPTION_ALIASES opt opt_ALIASES style)
  if(${opt_ALIASES})
    foreach(item ${${opt_ALIASES}})
      string(REPLACE "ENABLE_" "DISABLE_" inverse_item ${item})
      set(inverse_aliases ${inverse_aliases} ${inverse_item})
    endforeach(item ${${opt_ALIASES}})
    foreach(item ${${opt_ALIASES}})
      if(NOT "${item}" STREQUAL "${opt}")
	if(NOT "${${item}}" STREQUAL "")
	  set(${opt} ${${item}} CACHE STRING "setting ${opt} via alias ${item}" FORCE)
	  set(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
	  mark_as_advanced(${item})
	endif(NOT "${${item}}" STREQUAL "")
      endif(NOT "${item}" STREQUAL "${opt}")
    endforeach(item ${${opt_ALIASES}})
    # handle DISABLE forms of options
    foreach(item ${inverse_aliases})
      if(NOT "${item}" STREQUAL "${opt}")
	if(NOT "${${item}}" STREQUAL "")
	  set(invert_item ${${item}})
	  if("${${item}}" STREQUAL "ON")
	    set(invert_item "OFF")
	  ELSEif("${invert_item}" STREQUAL "OFF")
	    set(invert_item "ON")
	  endif("${${item}}" STREQUAL "ON")
	  set(${opt} ${invert_item} CACHE STRING "setting ${opt} via alias ${item}" FORCE)
	  set(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
	  mark_as_advanced(${item})
	endif(NOT "${${item}}" STREQUAL "")
      endif(NOT "${item}" STREQUAL "${opt}")
    endforeach(item ${inverse_aliases})
    foreach(item ${${opt_ALIASES}})
      string(TOLOWER ${item} alias_str)
      string(REPLACE "_" "-" alias_str "${alias_str}")
      string(REPLACE "enable" "disable" disable_str "${alias_str}")
      if("${style}" STREQUAL "ABS")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${alias_str})                options=\"$options -D${opt}=BUNDLED\";\n")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${disable_str})                options=\"$options -D${opt}=SYSTEM\";\n")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
      endif("${style}" STREQUAL "ABS")
      if("${style}" STREQUAL "BOOL")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${alias_str})                options=\"$options -D${opt}=ON\";\n")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${disable_str})                options=\"$options -D${opt}=OFF\";\n")
	file(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
      endif("${style}" STREQUAL "BOOL")
    endforeach(item ${${opt_ALIASES}})
  endif(${opt_ALIASES})
endmacro(OPTION_ALIASES)

# Write documentation description for BRL-CAD options
macro(OPTION_DESCRIPTION opt opt_ALIASES opt_DESCRIPTION)
  file(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "\n--- ${opt} ---\n")
  file(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${${opt_DESCRIPTION}}")

  set(ALIASES_LIST "\nAliases: ")
  foreach(item ${${opt_ALIASES}})
    set(ALIASES_LIST_TEST "${ALIASES_LIST}, ${item}")
    string(LENGTH ${ALIASES_LIST_TEST} LL)

    if(${LL} GREATER 80)
      file(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${ALIASES_LIST}\n")
      set(ALIASES_LIST "          ${item}")
    else(${LL} GREATER 80)
      if(NOT ${ALIASES_LIST} MATCHES "\nAliases")
	set(ALIASES_LIST "${ALIASES_LIST}, ${item}")
      else(NOT ${ALIASES_LIST} MATCHES "\nAliases")
	if(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
	  set(ALIASES_LIST "${ALIASES_LIST}, ${item}")
	else(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
	  set(ALIASES_LIST "${ALIASES_LIST} ${item}")
	endif(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
      endif(NOT ${ALIASES_LIST} MATCHES "\nAliases")
    endif(${LL} GREATER 80)
  endforeach(item ${${opt_ALIASES}})

  if(ALIASES_LIST)
    string(STRIP ALIASES_LIST ${ALIASES_LIST})
    if(ALIASES_LIST)
      file(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${ALIASES_LIST}")
    endif(ALIASES_LIST)
  endif(ALIASES_LIST)

  file(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "\n\n")
endmacro(OPTION_DESCRIPTION)

# Standard option with BRL-CAD aliases and description
macro(BRLCAD_OPTION default opt opt_ALIASES opt_DESCRIPTION)
  if(NOT DEFINED ${opt} OR "${${opt}}" STREQUAL "")
    if("${default}" STREQUAL "ON" OR "${default}" STREQUAL "YES")
      set(${opt} ON CACHE BOOL "define ${opt}" FORCE)
    ELSEif("${default}" STREQUAL "OFF" OR "${default}" STREQUAL "NO")
      set(${opt} OFF CACHE BOOL "define ${opt}" FORCE)
    else("${default}" STREQUAL "ON" OR "${default}" STREQUAL "YES")
      set(${opt} ${default} CACHE STRING "define ${opt}" FORCE)
    endif("${default}" STREQUAL "ON" OR "${default}" STREQUAL "YES")
  endif(NOT DEFINED ${opt} OR "${${opt}}" STREQUAL "")

  OPTION_ALIASES("${opt}" "${opt_ALIASES}" "BOOL")
  OPTION_DESCRIPTION("${opt}" "${opt_ALIASES}" "${opt_DESCRIPTION}")
endmacro(BRLCAD_OPTION)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
