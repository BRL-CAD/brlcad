#
# Try to find OPENMESH
# Once done this will define
#  
# OPENMESH_FOUND           - system has OPENMESH
# OPENMESH_INCLUDE_DIRS    - the OPENMESH include directories
# OPENMESH_LIBRARIES       - Link these to use OPENMESH
###
# - helpful cmake flags when building
#
#  OPENMESH_ROOT          - path to OpenMesh root if it is built outside of the usual location
#
#=============================================================================
#
# Copyright 2015 Computer Graphics Group, RWTH Aachen University
# Authors: Jan Möbius <moebius@cs.rwth-aachen.de>
#          Hans-Christian Ebke <ebke@cs.rwth-aachen.de>
#
#
#===========================================================================
#
#                               OpenMesh
#           Copyright (c) 2001-2015, RWTH-Aachen University
#           Department of Computer Graphics and Multimedia
#                          All rights reserved.
#                            www.openmesh.org
#
#---------------------------------------------------------------------------
# This file is part of OpenMesh.
#---------------------------------------------------------------------------
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#===========================================================================

#if already found via finder or simulated finder in openmesh CMakeLists.txt, skip the search
if(NOT OPENMESH_FOUND)
	set(SEARCH_PATHS
		"${OpenMesh_ROOT}"
		/usr/local/
		/usr/
		"${CMAKE_SOURCE_DIR}/OpenMesh/src/OpenMesh"
		"${CMAKE_SOURCE_DIR}/libs_required/OpenMesh/src/OpenMesh"
		"${CMAKE_SOURCE_DIR}/../OpenMesh/src/OpenMesh"
		"C:/Program Files/OpenMesh 8.1"
		"C:/libs/OpenMesh 8.1"
		"C:/libs/OpenMesh 8.0"
		"${OPENMESH_LIBRARY_DIR}"
		)

	foreach(sp ${SEARCH_PATHS})
		find_path(OPENMESH_INCLUDE_DIR NAMES OpenMesh/Core/Mesh/PolyMeshT.hh ${sp} PATH_SUFFIXES include)
		find_library(OPENMESH_CORE_LIBRARY_RELEASE NAMES OpenMeshCore ${sp} PATH_SUFFIXES lib lib64)
		find_library(OPENMESH_CORE_LIBRARY_DEBUG NAMES OpenMeshCored ${sp} PATH_SUFFIXES lib lib64)
		find_library(OPENMESH_TOOLS_LIBRARY_RELEASE NAMES OpenMeshTools ${sp} PATH_SUFFIXES lib lib64)
		find_library(OPENMESH_TOOLS_LIBRARY_DEBUG NAMES OpenMeshToolsd ${sp} PATH_SUFFIXES lib lib64)
	endforeach(sp ${SEARCH_PATHS})

	#select configuration depending on platform (optimized... on windows)
	include(SelectLibraryConfigurations)
	select_library_configurations(OPENMESH_TOOLS)
	select_library_configurations(OPENMESH_CORE)

	set(OPENMESH_LIBRARIES ${OPENMESH_CORE_LIBRARY} ${OPENMESH_TOOLS_LIBRARY} CACHE STRING "OpenMesh Core and Tools libraries" FORCE)
	set(OPENMESH_INCLUDE_DIRS ${OPENMESH_INCLUDE_DIR} CACHE STRING "OpenMesh include directory" FORCE)

	#checks, if OPENMESH was found and sets OPENMESH_FOUND if so
	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(OpenMesh  DEFAULT_MSG
		OPENMESH_CORE_LIBRARY OPENMESH_TOOLS_LIBRARY OPENMESH_INCLUDE_DIR)

	mark_as_advanced(OPENMESH_INCLUDE_DIR OPENMESH_CORE_LIBRARY_RELEASE OPENMESH_CORE_LIBRARY_DEBUG OPENMESH_TOOLS_LIBRARY_RELEASE OPENMESH_TOOLS_LIBRARY_DEBUG)
endif()

