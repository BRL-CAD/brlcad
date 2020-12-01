#             B R L C A D _ O P T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2020 United States Government as represented by
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


function(WRITE_CONFIG_YN opt opt_ALIASES onval offval)
  set(ofile "${CMAKE_BINARY_DIR}/configure.new")
  foreach(item ${opt_ALIASES})
    string(TOLOWER ${item} alias_str)
    string(REPLACE "_" "-" alias_str "${alias_str}")
    string(REPLACE "enable" "disable" disable_str "${alias_str}")
    file(APPEND "${ofile}" "     --${alias_str})                options=\"$options -D${opt}=${onval}\";\n")
    file(APPEND "${ofile}" "                                  shift;;\n")
    file(APPEND "${ofile}" "     --${disable_str})                options=\"$options -D${opt}=${offval}\";\n")
    file(APPEND "${ofile}" "                                  shift;;\n")
  endforeach(item ${opt_ALIASES})
endfunction(WRITE_CONFIG_YN)

function(WRITE_CONFIG_STR opt opt_ALIASES)
  foreach(item ${opt_ALIASES})
    string(TOLOWER ${item} alias_str)
    file(APPEND "${ofile}" "     --${alias_str}=*)               inputstr=$1;\n")
    file(APPEND "${ofile}" "                                     options=\"$options -D${opt}=\${inputstr#--${alias_str}=}\";\n")
    file(APPEND "${ofile}" "                                  shift;;\n")
  endforeach(item ${opt_ALIASES})
endfunction(WRITE_CONFIG_STR)

function(WRITE_CONFIG_OPTS opt opt_ALIASES style)

  set(ofile "${CMAKE_BINARY_DIR}/configure.new")

  if("${style}" STREQUAL "STRING")
    # If we've got a string type, we don't need inverse logic.
    WRITE_CONFIG_STR("${opt}" "${opt_ALIASES}")
    return()
  endif("${style}" STREQUAL "STRING")

  # handle BUNDLE/SYSTEM options
  if("${style}" STREQUAL "ABS")
    WRITE_CONFIG_YN("${opt}" "${opt_ALIASES}" BUNDLED SYSTEM)
  endif("${style}" STREQUAL "ABS")

  # handle Boolean options
  if("${style}" STREQUAL "BOOL")
    WRITE_CONFIG_YN("${opt}" "${opt_ALIASES}" "ON" "OFF")
  endif("${style}" STREQUAL "BOOL")

endfunction(WRITE_CONFIG_OPTS)

# Handle aliases for BRL-CAD options
function(OPTION_RESOLVE_ALIASES ropt opt opt_ALIASES style)

  set(lopt ${${ropt}})

  foreach(item ${opt_ALIASES})
    if(NOT "${item}" STREQUAL "${opt}" AND NOT "${${item}}" STREQUAL "")
      set(lopt ${${item}})
      set(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
      mark_as_advanced(${item})
    endif(NOT "${item}" STREQUAL "${opt}" AND NOT "${${item}}" STREQUAL "")
  endforeach(item ${opt_ALIASES})

  # String types don't have inverses
  if("${style}" STREQUAL "STRING")
    set(${ropt} ${lopt} PARENT_SCOPE)
    return()
  endif("${style}" STREQUAL "STRING")

  # Generate inverse aliases for all "ENABLE" options
  foreach(item ${opt_ALIASES})
    string(REPLACE "ENABLE_" "DISABLE_" inverse_item ${item})
    set(inverse_aliases ${inverse_aliases} ${inverse_item})
  endforeach(item ${opt_ALIASES})

  # Check the inverse options for a set value
  foreach(item ${opt_ALIASES})
    if(NOT "${${item}}" STREQUAL "")
      set(lopt ${${item}})
      set(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
      mark_as_advanced(${item})
    endif(NOT "${${item}}" STREQUAL "")
  endforeach(item ${opt_ALIASES})

  # Let the parent scope know the result
  set(${ropt} ${lopt} PARENT_SCOPE)

endfunction(OPTION_RESOLVE_ALIASES)

# Write documentation description for BRL-CAD options
function(WRITE_OPTION_DESCRIPTION opt opt_ALIASES opt_DESCRIPTION)

  set(ofile "${CMAKE_BINARY_DIR}/BRLCAD_OPTIONS")

  file(APPEND "${ofile}" "\n--- ${opt} ---\n")
  file(APPEND "${ofile}" "${${opt_DESCRIPTION}}")

  set(ALIASES_LIST "\nAliases: ")
  foreach(item ${opt_ALIASES})
    set(ALIASES_LIST_TEST "${ALIASES_LIST}, ${item}")
    string(LENGTH ${ALIASES_LIST_TEST} LL)

    if(${LL} GREATER 80)
      file(APPEND "${ofile}" "${ALIASES_LIST}\n")
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
  endforeach(item ${opt_ALIASES})

  if(ALIASES_LIST)
    string(STRIP ALIASES_LIST ${ALIASES_LIST})
    if(ALIASES_LIST)
      file(APPEND "${ofile}" "${ALIASES_LIST}")
    endif(ALIASES_LIST)
  endif(ALIASES_LIST)

  file(APPEND "${ofile}" "\n\n")

endfunction(WRITE_OPTION_DESCRIPTION)


function(VAL_NORMALIZE val optype)
  string(TOUPPER "${${val}}" uval)
  if("${uval}" STREQUAL "YES")
    set(uval "ON")
  endif("${uval}" STREQUAL "YES")
  if("${uval}" STREQUAL "NO")
    set(uval "OFF")
  endif("${uval}" STREQUAL "NO")
  if("${optype}" STREQUAL "ABS")
    if("${uval}" STREQUAL "ON")
      set(uval "BUNDLED")
    endif("${uval}" STREQUAL "ON")
    if("${uval}" STREQUAL "OFF")
      set(uval "SYSTEM")
    endif("${uval}" STREQUAL "OFF")
  endif("${optype}" STREQUAL "ABS")
  set(${val} "${uval}" PARENT_SCOPE)
endfunction(VAL_NORMALIZE)


function(OPT_NORMALIZE ropt default optype)

  # We need something for a default
  if("${default}" STREQUAL "")
    message(FATAL_ERROR "Error - empty default value supplied for ${opt}")
  endif("${default}" STREQUAL "")

  # If we need to use the default, do so.
  if("${${ropt}}" STREQUAL "")
    set(local_opt "${default}")
  else("${${ropt}}" STREQUAL "")
    set(local_opt ${${ropt}})
  endif("${${ropt}}" STREQUAL "")

  # We should be done with logic that will change the setting - normalize
  VAL_NORMALIZE(local_opt ${optype})

  # Let the parent know what the normalized result is
  set(${ropt} ${local_opt} PARENT_SCOPE)

endfunction(OPT_NORMALIZE)


# Standard option with BRL-CAD aliases and description
function(BRLCAD_OPTION opt default)

  cmake_parse_arguments(O "" "DESCRIPTION;TYPE" "ALIASES" ${ARGN})

  # Initialize
  set(lopt "${${opt}}")
  set(otype)
  if(O_TYPE)
    set(otype ${O_TYPE})
  endif(O_TYPE)

  # Process aliases and write descriptions
  if(O_ALIASES)
    OPTION_RESOLVE_ALIASES(lopt "${opt}" "${O_ALIASES}" "${otype}")
    WRITE_CONFIG_OPTS("${opt}" "${O_ALIASES}" "${otype}")
    if(O_DESCRIPTION)
      WRITE_OPTION_DESCRIPTION("${opt}" "${O_ALIASES}" "${O_DESCRIPTION}")
    endif(O_DESCRIPTION)
  endif(O_ALIASES)

  # Finalize the actual value to be used
  OPT_NORMALIZE(lopt "${default}" ${otype})

  # Set in the cache
  if("${otype}" STREQUAL "BOOL")
    set(${opt} "${lopt}" CACHE BOOL "define ${opt}" FORCE)
  else("${otype}" STREQUAL "BOOL")
    set(${opt} "${lopt}" CACHE STRING "define ${opt}" FORCE)
  endif("${otype}" STREQUAL "BOOL")

  # For drop-down menus in CMake gui - set STRINGS property
  if("${otype}" STREQUAL "ABS")
    set_property(CACHE ${opt} PROPERTY STRINGS AUTO BUNDLED SYSTEM)
  endif("${otype}" STREQUAL "ABS")

  # Let the parent know
  set(${opt} "${lopt}" PARENT_SCOPE)

endfunction(BRLCAD_OPTION)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
