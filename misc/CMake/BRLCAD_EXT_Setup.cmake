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

include(CMakeParseArguments)

# Build a local copy of bext if we were unable to locate one

function(brlcad_ext_setup)

  set(BRLCAD_EXT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/bext_build)
  set(BRLCAD_EXT_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR})

  # If we don't have
  if (NOT DEFINED BRLCAD_EXT_SOURCE_DIR)
    set(BRLCAD_EXT_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/bext)
    if (NOT EXISTS ${BRLCAD_EXT_SOURCE_DIR})
      find_program(GIT_EXEC git REQUIRED)
      execute_process(COMMAND ${GIT_EXEC} clone https://github.com/BRL-CAD/bext.git)
    endif (NOT EXISTS ${BRLCAD_EXT_SOURCE_DIR})
  endif (NOT DEFINED BRLCAD_EXT_SOURCE_DIR)
  if (NOT EXISTS ${BRLCAD_EXT_SOURCE_DIR})
    message(FATAL_ERROR "bext directory ${BRLCAD_EXT_SOURCE_DIR} is not present")
  endif (NOT EXISTS ${BRLCAD_EXT_SOURCE_DIR})

  if (NOT EXISTS ${BRLCAD_EXT_BUILD_DIR})
    file(MAKE_DIRECTORY ${BRLCAD_EXT_BUILD_DIR})
  endif (NOT EXISTS ${BRLCAD_EXT_BUILD_DIR})

  # Need to control options for this based on BRL-CAD configure settings.
  # Unlike an independent bext build, we know for this one what we can turn on
  # and off
  set(BRLCAD_EXT_CMAKE_OPTS "-DGIT_SHALLOW_CLONE=ON")
  if ("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
    set(BRLCAD_EXT_CMAKE_OPTS ${BRLCAD_EXT_CMAKE_OPTS} "-DENABLE_ALL=ON")
  endif ("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
  if (NOT BRLCAD_ENABLE_GDAL)
    set(BRLCAD_EXT_CMAKE_OPTS ${BRLCAD_EXT_CMAKE_OPTS} "-DUSE_GDAL=OFF")
  endif (NOT BRLCAD_ENABLE_GDAL)
  if (NOT BRLCAD_ENABLE_QT)
    set(BRLCAD_EXT_CMAKE_OPTS ${BRLCAD_EXT_CMAKE_OPTS} "-DUSE_QT=OFF")
  endif (NOT BRLCAD_ENABLE_QT)
  if (NOT BRLCAD_ENABLE_TCL)
    set(BRLCAD_EXT_CMAKE_OPTS ${BRLCAD_EXT_CMAKE_OPTS} "-DUSE_TCL=OFF")
  endif (NOT BRLCAD_ENABLE_TCL)

  if (BRLCAD_COMPONENTS)
    set(active_dirs ${BRLCAD_COMPONENTS})
    foreach(wc ${BRLCAD_COMPONENTS})
      deps_expand(${wc} active_dirs)
    endforeach(wc ${BRLCAD_COMPONENTS})
    message("${CMAKE_COMMAND} ${BRLCAD_EXT_SOURCE_DIR} ${BRLCAD_EXT_CMAKE_OPTS} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${BRLCAD_EXT_INSTALL_DIR} -DBRLCAD_COMPONENTS=${active_dirs}")
    execute_process(COMMAND ${CMAKE_COMMAND} ${BRLCAD_EXT_SOURCE_DIR}
      ${BRLCAD_EXT_CMAKE_OPTS}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_INSTALL_PREFIX=${BRLCAD_EXT_INSTALL_DIR}
      -DBRLCAD_COMPONENTS=${active_dirs}
      WORKING_DIRECTORY ${BRLCAD_EXT_BUILD_DIR}
      )
  else (BRLCAD_COMPONENTS)
    execute_process(COMMAND ${CMAKE_COMMAND} ${BRLCAD_EXT_SOURCE_DIR}
      ${BRLCAD_EXT_CMAKE_OPTS}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_INSTALL_PREFIX=${BRLCAD_EXT_INSTALL_DIR}
      WORKING_DIRECTORY ${BRLCAD_EXT_BUILD_DIR}
      )
  endif (BRLCAD_COMPONENTS)

  if (CMAKE_CONFIGURATION_TYPES)
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BRLCAD_EXT_BUILD_DIR} --parallel 8 --config ${CMAKE_BUILD_TYPE} WORKING_DIRECTORY ${BRLCAD_EXT_BUILD_DIR})
  else (CMAKE_CONFIGURATION_TYPES)
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BRLCAD_EXT_BUILD_DIR} --parallel 8 WORKING_DIRECTORY ${BRLCAD_EXT_BUILD_DIR})
  endif (CMAKE_CONFIGURATION_TYPES)

  set(BRLCAD_EXT_DIR ${BRLCAD_EXT_INSTALL_DIR}/bext_output CACHE PATH "Local bext install" FORCE)
  set(BRLCAD_EXT_INSTALL_DIR ${BRLCAD_EXT_DIR}/install CACHE PATH "Local bext install" FORCE)
  set(BRLCAD_EXT_NOINSTALL_DIR ${BRLCAD_EXT_DIR}/noinstall CACHE PATH "Local bext install" FORCE)
endfunction(brlcad_ext_setup)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
