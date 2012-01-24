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
macro(CMAKEFILES)
  if(NOT IS_SUBBUILD)
    foreach(ITEM ${ARGN})
      if(NOT ${ITEM} MATCHES "^SHARED$" AND NOT ${ITEM} MATCHES "^STATIC$" AND NOT x${ITEM} MATCHES "^xWIN32$")
	get_filename_component(ITEM_PATH ${ITEM} PATH)
	get_filename_component(ITEM_NAME ${ITEM} NAME)
	if(NOT ITEM_PATH STREQUAL "")
	  get_filename_component(ITEM_ABS_PATH ${ITEM_PATH} ABSOLUTE)
	  if(NOT ${ITEM_PATH} MATCHES "^${ITEM_ABS_PATH}$")
	    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	      message(FATAL_ERROR "Attempting to ignore non-existent file ${ITEM}, in directory ${CMAKE_CURRENT_SOURCE_DIR}")
	    endif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	      file(APPEND	${CMAKE_BINARY_DIR}/cmakedirs.cmake	"${ITEM_ABS_PATH}/${ITEM_NAME}\n")
	    else(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	      file(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
	    endif(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	    file(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")
	    while(NOT ITEM_PATH STREQUAL "")
	      get_filename_component(ITEM_NAME	${ITEM_PATH} NAME)
	      get_filename_component(ITEM_PATH	${ITEM_PATH} PATH)
	      if(NOT ITEM_PATH STREQUAL "")
		get_filename_component(ITEM_ABS_PATH ${ITEM_PATH} ABSOLUTE)
		if(NOT ${ITEM_NAME} MATCHES "\\.\\.")
		  file(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")
		endif(NOT ${ITEM_NAME} MATCHES "\\.\\.")
	      endif(NOT ITEM_PATH STREQUAL "")
	    endwhile(NOT ITEM_PATH STREQUAL "")
	  endif(NOT ${ITEM_PATH} MATCHES "^${ITEM_ABS_PATH}$")
	else(NOT ITEM_PATH STREQUAL "")
	  if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	    message(FATAL_ERROR "Attempting to ignore non-existent file ${ITEM}, in directory ${CMAKE_CURRENT_SOURCE_DIR}")
	  endif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	    file(APPEND	${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM_NAME}\n")
	  else(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	    file(APPEND	${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM_NAME}\n")
	  endif(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM})
	endif(NOT ITEM_PATH STREQUAL "")
      endif()
    endforeach(ITEM ${ARGN})
  endif(NOT IS_SUBBUILD)
endmacro(CMAKEFILES FILESLIST)

# Routine to tell distcheck to ignore a single item (can be a directory)
macro(DISTCHECK_IGNORE_ITEM itemtoignore)
  if(NOT IS_SUBBUILD)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
      if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
	file(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore}\n")
      else(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
	file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore}\n")
      endif(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
    else(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
      message(FATAL_ERROR "Attempting to ignore non-existent file ${itemtoignore}")
    endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
  endif(NOT IS_SUBBUILD)
endmacro(DISTCHECK_IGNORE_ITEM)


# Routine to tell distcheck to ignore a series of items in a directory.  Items may themselves
# be directories.  Primarily useful when working with src/other dist lists.
macro(DISTCHECK_IGNORE targetdir filestoignore)
  if(NOT IS_SUBBUILD)
    foreach(ITEM ${${filestoignore}})
      get_filename_component(ITEM_PATH ${ITEM} PATH)
      get_filename_component(ITEM_NAME ${ITEM} NAME)

      if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	if(NOT ITEM_PATH STREQUAL "")
	  get_filename_component(ITEM_ABS_PATH ${targetdir}/${ITEM_PATH} ABSOLUTE)

	  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	    file(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
	  else(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	    file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
	  endif(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})

	  file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")

	  while(NOT ITEM_PATH STREQUAL "")
	    get_filename_component(ITEM_NAME ${ITEM_PATH} NAME)
	    get_filename_component(ITEM_PATH ${ITEM_PATH} PATH)
	    if(NOT ITEM_PATH STREQUAL "")
	      get_filename_component(ITEM_ABS_PATH ${targetdir}/${ITEM_PATH} ABSOLUTE)
	      if(NOT ${ITEM_NAME} MATCHES "\\.\\.")
		file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")
	      endif(NOT ${ITEM_NAME} MATCHES "\\.\\.")
	    endif(NOT ITEM_PATH STREQUAL "")
	  endwhile(NOT ITEM_PATH STREQUAL "")
	else(NOT ITEM_PATH STREQUAL "")
	  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	    file(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}\n")
	  else(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	    file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}\n")
	  endif(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	endif(NOT ITEM_PATH STREQUAL "")
      else(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	message(FATAL_ERROR "Attempting to ignore non-existent file ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}")
      endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
    endforeach(ITEM ${${filestoignore}})
  endif(NOT IS_SUBBUILD)
endmacro(DISTCHECK_IGNORE)


