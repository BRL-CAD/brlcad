# Copyright (c) 2010-2013 United States Government as represented by
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

# The location in which to install BRL-CAD executables.
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

# The location in which to install BRL-CAD header files.
if(NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT INCLUDE_DIR)

# The location in which to install BRL-CAD libraries.
if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)

# The location in which to install BRL-CAD configuration files.
if(NOT CONF_DIR)
  set(CONF_DIR etc)
endif(NOT CONF_DIR)

# The location in which to install BRL-CAD data files
if(NOT DATA_DIR)
  set(DATA_DIR share)
endif(NOT DATA_DIR)

# The location in which to install BRL-CAD Manual pages
if(NOT MAN_DIR)
  set(MAN_DIR ${DATA_DIR}/man)
endif(NOT MAN_DIR)

# The location in which to install BRL-CAD documentation files
if(NOT DOC_DIR)
  set(DOC_DIR ${DATA_DIR}/doc)
endif(NOT DOC_DIR)

# Make sure no absolute paths have been supplied to these variables
set(INSTALL_DIRS BIN INCLUDE LIB CONF DATA MAN DOC)
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

if(NOT CMAKE_CONFIGURATION_TYPES)
  # If we're not doing multi-configuration, just set the three main
  # variables to the correct values.
  if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all libraries.")
  endif(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all archives.")
  endif(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${BIN_DIR} CACHE INTERNAL "Single output directory for building all executables.")
  endif(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
else(NOT CMAKE_CONFIGURATION_TYPES)
  # Multi-configuration is more difficult.  Not only do we need to
  # properly set the output directories, but we also need to
  # identify the "toplevel" directory for each configuration so
  # we can place files, documentation, etc. in the correct
  # relative positions.  Because files may be placed by CMake
  # without a build target to put them in their proper relative build
  # directory position using these paths, we must fully qualify them
  # without using CMAKE_CFG_INTDIR.
  #
  # We define directories that may not be quite "standard"
  # for a particular build tool - for example, native VS2010 projects use
  # another directory to denote CPU type being compiled for - but CMake only
  # supports multi-configuration setups having multiple configurations,
  # not multiple compilers.
  #
  # One additional wrinkle we must watch for here is the case where
  # a multi-configuration setup uses "." for its internal directory -
  # if that's the case, we need to just set the various config output
  # directories to the same value.
  set(CFG_ROOT ${${PROJECT_NAME}_BINARY_DIR})
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    if(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
      set(CFG_ROOT ${${PROJECT_NAME}_BINARY_DIR}/${CFG_TYPE})
    endif(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    if(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
      set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CFG_ROOT}/${LIB_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} libraries.")
    endif(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
    if(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
      set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CFG_ROOT}/${LIB_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} archives.")
    endif(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
    if(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
      set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CFG_ROOT}/${BIN_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} executables.")
    endif(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
    if(NOT "CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}")
      set("CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}" ${CFG_ROOT} CACHE INTERNAL "Toplevel binary dir for ${CFG_TYPE} building.")
    endif(NOT "CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}")
    if(NOT "${PROJECT_NAME}_BINARY_DIR_${CFG_TYPE_UPPER}")
      set("${PROJECT_NAME}_BINARY_DIR_${CFG_TYPE_UPPER}" ${CFG_ROOT} CACHE INTERNAL "Toplevel binary dir for ${CFG_TYPE} building.")
    endif(NOT "${PROJECT_NAME}_BINARY_DIR_${CFG_TYPE_UPPER}")
  endforeach()
endif(NOT CMAKE_CONFIGURATION_TYPES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
