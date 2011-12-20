#             B R L C A D _ O P T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
MACRO(AUTO_OPTION username varname debug_state release_state)
	STRING(LENGTH "${${username}}" ${username}_SET)

	IF(NOT ${username}_SET)
		SET(${username} "AUTO" CACHE STRING "auto option" FORCE)
	ENDIF(NOT ${username}_SET)

	set_property(CACHE ${username} PROPERTY STRINGS AUTO "ON" "OFF")

	STRING(TOUPPER "${${username}}" username_upper)
	SET(${username} ${username_upper})

	IF(NOT ${${username}} MATCHES "AUTO" AND NOT ${${username}} MATCHES "ON" AND NOT ${${username}} MATCHES "OFF")
		MESSAGE(WARNING "Unknown value ${${username}} supplied for ${username} - defaulting to AUTO.\nValid options are AUTO, ON and OFF")
		SET(${username} "AUTO" CACHE STRING "auto option" FORCE)
	ENDIF(NOT ${${username}} MATCHES "AUTO" AND NOT ${${username}} MATCHES "ON" AND NOT ${${username}} MATCHES "OFF")

	# If the "parent" setting isn't AUTO, do what it says
	IF(NOT ${${username}} MATCHES "AUTO")
		SET(${varname} ${${username}})
	ENDIF(NOT ${${username}} MATCHES "AUTO")

	# If we we don't understand the build type and have an AUTO setting for the
	# optimization flags, leave them off
	IF(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
		IF(NOT ${${username}} MATCHES "AUTO")
			SET(${varname} ${${username}})
		ELSE(NOT ${${username}} MATCHES "AUTO")
			SET(${varname} OFF)
			SET(${username} "OFF (AUTO)" CACHE STRING "auto option" FORCE)
		ENDIF(NOT ${${username}} MATCHES "AUTO")
	ENDIF(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")

	# If we DO understand the build type and have AUTO, be smart
	IF("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${${username}} MATCHES "AUTO")
		SET(${varname} ${release_state})
		SET(${username} "${release_state} (AUTO)" CACHE STRING "auto option" FORCE)
	ENDIF("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${${username}} MATCHES "AUTO")

	IF("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${${username}} MATCHES "AUTO")
		SET(${varname} ${debug_state})
		SET(${username} "${debug_state} (AUTO)" CACHE STRING "auto option" FORCE)
	ENDIF("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${${username}} MATCHES "AUTO")
ENDMACRO(AUTO_OPTION varname release_state debug_state)

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
MACRO(OPTION_ALIASES opt opt_ALIASES style)
    IF(${opt_ALIASES})
	FOREACH(item ${${opt_ALIASES}})
	    STRING(REPLACE "ENABLE_" "DISABLE_" inverse_item ${item})
	    SET(inverse_aliases ${inverse_aliases} ${inverse_item})
	ENDFOREACH(item ${${opt_ALIASES}})
	FOREACH(item ${${opt_ALIASES}})
	    IF(NOT "${item}" STREQUAL "${opt}")
		IF(NOT "${${item}}" STREQUAL "")
		    SET(${opt} ${${item}} CACHE STRING "setting ${opt} via alias ${item}" FORCE)
		    SET(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
		    MARK_AS_ADVANCED(${item})
		ENDIF(NOT "${${item}}" STREQUAL "")
	    ENDIF(NOT "${item}" STREQUAL "${opt}")
	ENDFOREACH(item ${${opt_ALIASES}})
	# handle DISABLE forms of options
	FOREACH(item ${inverse_aliases})
	    IF(NOT "${item}" STREQUAL "${opt}")
		IF(NOT "${${item}}" STREQUAL "")
		    SET(invert_item ${${item}})
		    IF("${${item}}" STREQUAL "ON")
			SET(invert_item "OFF")
		    ELSEIF("${invert_item}" STREQUAL "OFF")
			SET(invert_item "ON")
		    ENDIF("${${item}}" STREQUAL "ON")
		    SET(${opt} ${invert_item} CACHE STRING "setting ${opt} via alias ${item}" FORCE)
		    SET(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
		    MARK_AS_ADVANCED(${item})
		ENDIF(NOT "${${item}}" STREQUAL "")
	    ENDIF(NOT "${item}" STREQUAL "${opt}")
	ENDFOREACH(item ${inverse_aliases})
	FOREACH(item ${${opt_ALIASES}})
	    STRING(TOLOWER ${item} alias_str)
	    STRING(REPLACE "_" "-" alias_str "${alias_str}")
	    STRING(REPLACE "enable" "disable" disable_str "${alias_str}")
	    IF("${style}" STREQUAL "ABS")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${alias_str})                options=\"$options -D${opt}=BUNDLED\";\n")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${disable_str})                options=\"$options -D${opt}=SYSTEM\";\n")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
	    ENDIF("${style}" STREQUAL "ABS")
	    IF("${style}" STREQUAL "BOOL")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${alias_str})                options=\"$options -D${opt}=ON\";\n")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "     --${disable_str})                options=\"$options -D${opt}=OFF\";\n")
		FILE(APPEND ${CMAKE_BINARY_DIR}/configure.new "                                  shift;;\n")
	    ENDIF("${style}" STREQUAL "BOOL")
	ENDFOREACH(item ${${opt_ALIASES}})
    ENDIF(${opt_ALIASES})
ENDMACRO(OPTION_ALIASES)

# Write documentation description for BRL-CAD options
MACRO(OPTION_DESCRIPTION opt opt_ALIASES opt_DESCRIPTION)
    FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "\n--- ${opt} ---\n")
    FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${${opt_DESCRIPTION}}")

    SET(ALIASES_LIST "\nAliases: ")
    FOREACH(item ${${opt_ALIASES}})
	SET(ALIASES_LIST_TEST "${ALIASES_LIST}, ${item}")
	STRING(LENGTH ${ALIASES_LIST_TEST} LL)

	IF(${LL} GREATER 80)
	    FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${ALIASES_LIST}\n")
	    SET(ALIASES_LIST "          ${item}")
	ELSE(${LL} GREATER 80)
	    IF(NOT ${ALIASES_LIST} MATCHES "\nAliases")
		SET(ALIASES_LIST "${ALIASES_LIST}, ${item}")
	    ELSE(NOT ${ALIASES_LIST} MATCHES "\nAliases")
		IF(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
		    SET(ALIASES_LIST "${ALIASES_LIST}, ${item}")
		ELSE(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
		    SET(ALIASES_LIST "${ALIASES_LIST} ${item}")
		ENDIF(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
	    ENDIF(NOT ${ALIASES_LIST} MATCHES "\nAliases")
	ENDIF(${LL} GREATER 80)
    ENDFOREACH(item ${${opt_ALIASES}})

    IF(ALIASES_LIST)	
	STRING(STRIP ALIASES_LIST ${ALIASES_LIST})
	IF(ALIASES_LIST)	
	    FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${ALIASES_LIST}")
	ENDIF(ALIASES_LIST)	
    ENDIF(ALIASES_LIST)	

    FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "\n\n")
ENDMACRO(OPTION_DESCRIPTION)

# Standard option with BRL-CAD aliases and description
MACRO(BRLCAD_OPTION default opt opt_ALIASES opt_DESCRIPTION)
    IF(NOT DEFINED ${opt})
	SET(${opt} ${default} CACHE STRING "define ${opt}" FORCE)
    ENDIF(NOT DEFINED ${opt})
    IF("${${opt}}" STREQUAL "")
	SET(${opt} ${default} CACHE STRING "define ${opt}" FORCE)
    ENDIF("${${opt}}" STREQUAL "")

    OPTION_ALIASES("${opt}" "${opt_ALIASES}" "BOOL")
    OPTION_DESCRIPTION("${opt}" "${opt_ALIASES}" "${opt_DESCRIPTION}")
ENDMACRO(BRLCAD_OPTION)

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
