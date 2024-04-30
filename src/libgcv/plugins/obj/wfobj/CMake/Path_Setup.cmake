# Copyright (c) 2010-2024 United States Government as represented by
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


#---------------------------------------------------------------------
# Define relative install locations.  Don't set these if they have already
# been set by some other means (like a higher level CMakeLists.txt file
# including this one).

# Note - Windows has different constraints than other platforms on the
# relative placement of libraries and binaries.  We don't know of a
# proven way to make the bin/lib layout work on Windows the way we do
# on other platforms yet - the best candidate is probably:
#
# https://stackoverflow.com/a/2003775/2037687
#
# but it's implications for behavior and performance (or even whether
# it can succeed at all with our code) are unknown.

# The location in which to install BRL-CAD executables.
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

# Define a relative path that will "reset" a path back to
# the point before BIN_DIR was appended.  This is primarily
# useful when working with generator expressions
unset(RBIN_DIR CACHE)
set(LBIN_DIR "${BIN_DIR}")
while (NOT "${LBIN_DIR}" STREQUAL "")
  get_filename_component(LBDIR "${LBIN_DIR}" DIRECTORY)
  set(LBIN_DIR "${LBDIR}")
  if ("${RBIN_DIR}" STREQUAL "")
    set(RBIN_DIR "..")
  else ("${RBIN_DIR}" STREQUAL "")
    set(RBIN_DIR "../${RBIN_DIR}")
  endif ("${RBIN_DIR}" STREQUAL "")
endwhile (NOT "${LBIN_DIR}" STREQUAL "")

# The location in which to install BRL-CAD libraries.
if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)
if(NOT LIBEXEC_DIR)
  set(LIBEXEC_DIR libexec)
endif(NOT LIBEXEC_DIR)

# The location in which to install BRL-CAD header files.
if(NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT INCLUDE_DIR)

# The location in which to install BRL-CAD data files
if(NOT DATA_DIR)
  set(DATA_DIR share)
endif(NOT DATA_DIR)

# The location in which to install BRL-CAD documentation files
if(NOT DOC_DIR)
  set(DOC_DIR ${DATA_DIR}/doc)
endif(NOT DOC_DIR)

# The location in which to install BRL-CAD Manual pages
if(NOT MAN_DIR)
  set(MAN_DIR ${DATA_DIR}/man)
endif(NOT MAN_DIR)

# Make sure no absolute paths have been supplied to these variables
set(INSTALL_DIRS BIN INCLUDE LIB LIBEXEC DATA MAN DOC)
foreach(instdir ${INSTALL_DIRS})
  get_filename_component(instdir_full ${${instdir}_DIR} ABSOLUTE)
  if("${${instdir}_DIR}" STREQUAL "${instdir_full}")
    message(FATAL_ERROR "Error - absolute path supplied for ${instdir}_DIR.  This path must be relative - e.g. \"bin\" instead of \"/usr/bin\"")
    set(HAVE_INSTALL_DIR_FULL_PATH 1)
  endif("${${instdir}_DIR}" STREQUAL "${instdir_full}")
endforeach(instdir ${INSTALL_DIRS})

#---------------------------------------------------------------------
# Output directories - this is where built library and executable
# files will be placed after building but prior to install.  The
# necessary variables change between single and multi configuration
# build systems, so it is necessary to handle both cases on a
# conditional basis.

# Set the three main variables to the correct values.
if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all libraries.")
endif(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all archives.")
endif(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${BIN_DIR} CACHE INTERNAL "Single output directory for building all executables.")
endif(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)

if (CMAKE_CONFIGURATION_TYPES)
  # If the generator thinks it's doing multiconfig, we need to override its
  # output variables to avoid the use of subdirectories
  set(CFG_ROOT ${${PROJECT_NAME}_BINARY_DIR})
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} CACHE INTERNAL "Single output directory for building all libraries.")
    set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} CACHE INTERNAL "Single output directory for building all archives.")
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} CACHE INTERNAL "Single output directory for building all executables.")
    set("CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}" ${CMAKE_BINARY_DIR} CACHE INTERNAL "Toplevel binary dir for all building.")
    set("${PROJECT_NAME}_BINARY_DIR_${CFG_TYPE_UPPER}" ${CMAKE_BINARY_DIR} CACHE INTERNAL "Toplevel binary dir for all building.")
  endforeach()
endif(CMAKE_CONFIGURATION_TYPES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
