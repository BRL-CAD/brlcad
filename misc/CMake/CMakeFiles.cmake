#               C M A K E F I L E S . C M A K E
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

# Define a macro for building lists of files.  Distcheck needs
# to know what files are "supposed" to be present in order to make
# sure the source tree is clean prior to building a distribution
# tarball, hence this macro stores its results in files and not
# variables  It's a no-op in a SUBBUILD.
MACRO(CMAKEFILES)
	IF(NOT IS_SUBBUILD)
		FOREACH(ITEM ${ARGN})
			IF(NOT ${ITEM} MATCHES "^SHARED$" AND NOT ${ITEM} MATCHES "^STATIC$" AND NOT x${ITEM} MATCHES "^xWIN32$")
				GET_FILENAME_COMPONENT(ITEM_PATH ${ITEM} PATH)
				GET_FILENAME_COMPONENT(ITEM_NAME ${ITEM} NAME)
				IF(NOT ITEM_PATH STREQUAL "")
					GET_FILENAME_COMPONENT(ITEM_ABS_PATH ${ITEM_PATH} ABSOLUTE)
					IF(NOT ${ITEM_PATH} MATCHES "^${ITEM_ABS_PATH}$")
						IF(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
							MESSAGE(FATAL_ERROR "Attempting to ignore non-existent file ${ITEM}, in directory ${CMAKE_CURRENT_SOURCE_DIR}")
						ENDIF(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
						IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
							FILE(APPEND	${CMAKE_BINARY_DIR}/cmakedirs.cmake	"${ITEM_ABS_PATH}/${ITEM_NAME}\n")
						ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
							FILE(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
						ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
						FILE(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")
						WHILE(NOT ITEM_PATH STREQUAL "")
							get_filename_component(ITEM_NAME	${ITEM_PATH} NAME)
							get_filename_component(ITEM_PATH	${ITEM_PATH} PATH)
							IF(NOT ITEM_PATH STREQUAL "")
								GET_FILENAME_COMPONENT(ITEM_ABS_PATH ${ITEM_PATH} ABSOLUTE)
								IF(NOT ${ITEM_NAME} MATCHES "\\.\\.")
									FILE(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")
								ENDIF(NOT ${ITEM_NAME} MATCHES "\\.\\.")
							ENDIF(NOT ITEM_PATH STREQUAL "")
						ENDWHILE(NOT ITEM_PATH STREQUAL "")
					ENDIF(NOT ${ITEM_PATH} MATCHES "^${ITEM_ABS_PATH}$")
				ELSE(NOT ITEM_PATH STREQUAL "")
					IF(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
						MESSAGE(FATAL_ERROR "Attempting to ignore non-existent file ${ITEM}, in directory ${CMAKE_CURRENT_SOURCE_DIR}")
					ENDIF(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
					IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
						FILE(APPEND	${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM_NAME}\n")
					ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
						FILE(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM_NAME}\n")
					ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
				ENDIF(NOT ITEM_PATH STREQUAL "")
			ENDIF()
		ENDFOREACH(ITEM ${ARGN})
	ENDIF(NOT IS_SUBBUILD)
ENDMACRO(CMAKEFILES FILESLIST)

# Routine to tell distcheck to ignore a single item (can be a directory)
MACRO(DISTCHECK_IGNORE_ITEM itemtoignore)
	IF(NOT IS_SUBBUILD)
		IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
			IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
				FILE(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore}\n")
			ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
				FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore}\n")
			ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
		ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
			MESSAGE(FATAL_ERROR "Attempting to ignore non-existent file ${itemtoignore}")
		ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
	ENDIF(NOT IS_SUBBUILD)
ENDMACRO(DISTCHECK_IGNORE_ITEM)


# Routine to tell distcheck to ignore a series of items in a directory.  Items may themselves
# be directories.  Primarily useful when working with src/other dist lists.
MACRO(DISTCHECK_IGNORE targetdir filestoignore)
	IF(NOT IS_SUBBUILD)
		FOREACH(ITEM ${${filestoignore}})
			get_filename_component(ITEM_PATH ${ITEM} PATH)
			get_filename_component(ITEM_NAME ${ITEM} NAME)

			IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
				IF(NOT ITEM_PATH STREQUAL "")
					GET_FILENAME_COMPONENT(ITEM_ABS_PATH ${targetdir}/${ITEM_PATH} ABSOLUTE)

					IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
						FILE(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
					ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
						FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
					ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})

					FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")

					WHILE(NOT ITEM_PATH STREQUAL "")
						get_filename_component(ITEM_NAME ${ITEM_PATH} NAME)
						get_filename_component(ITEM_PATH ${ITEM_PATH} PATH)
						IF(NOT ITEM_PATH STREQUAL "")
							GET_FILENAME_COMPONENT(ITEM_ABS_PATH ${targetdir}/${ITEM_PATH} ABSOLUTE)
							IF(NOT ${ITEM_NAME} MATCHES "\\.\\.")
								FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")
							ENDIF(NOT ${ITEM_NAME} MATCHES "\\.\\.")
						ENDIF(NOT ITEM_PATH STREQUAL "")
					ENDWHILE(NOT ITEM_PATH STREQUAL "")
				ELSE(NOT ITEM_PATH STREQUAL "")
					IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
						FILE(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}\n")
					ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
						FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}\n")
					ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
				ENDIF(NOT ITEM_PATH STREQUAL "")
			ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
				MESSAGE(FATAL_ERROR "Attempting to ignore non-existent file ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}")
			ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
		ENDFOREACH(ITEM ${${filestoignore}})
	ENDIF(NOT IS_SUBBUILD)
ENDMACRO(DISTCHECK_IGNORE)


