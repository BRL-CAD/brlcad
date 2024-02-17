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

function(bext_setup)

  set(BEXT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/bext_build)
  set(BEXT_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR})

  # If we don't have
  if (NOT DEFINED BEXT_SOURCE_DIR)
    set(BEXT_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/bext)
    if (NOT EXISTS ${BEXT_SOURCE_DIR})
      find_program(GIT_EXEC git REQUIRED)
      execute_process(COMMAND ${GIT_EXEC} clone https://github.com/BRL-CAD/bext.git)
    endif (NOT EXISTS ${BEXT_SOURCE_DIR})
  endif (NOT DEFINED BEXT_SOURCE_DIR)
  if (NOT EXISTS ${BEXT_SOURCE_DIR})
    message(FATAL_ERROR "bext directory ${BEXT_SOURCE_DIR} is not present")
  endif (NOT EXISTS ${BEXT_SOURCE_DIR})

  if (NOT EXISTS ${BEXT_BUILD_DIR})
    file(MAKE_DIRECTORY ${BEXT_BUILD_DIR})
  endif (NOT EXISTS ${BEXT_BUILD_DIR})

  # TODO - Need to control options for this based on BRL-CAD configure
  # settings.  Unlike an independent bext build, we know for this one what
  # we can turn on and off
  set(BEXT_CMAKE_OPTS)
  if ("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
    set(BEXT_CMAKE_OPTS ${BEXT_CMAKE_OPTS} "-DENABLE_ALL=ON")
  endif ("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
  if (NOT BRLCAD_ENABLE_GDAL)
    set(BEXT_CMAKE_OPTS ${BEXT_CMAKE_OPTS} "-DUSE_GDAL=OFF")
  endif (NOT BRLCAD_ENABLE_GDAL)
  if (NOT BRLCAD_ENABLE_QT)
    set(BEXT_CMAKE_OPTS ${BEXT_CMAKE_OPTS} "-DUSE_QT=OFF")
  endif (NOT BRLCAD_ENABLE_QT)
  if (NOT BRLCAD_ENABLE_TCL)
    set(BEXT_CMAKE_OPTS ${BEXT_CMAKE_OPTS} "-DUSE_TCL=OFF")
  endif (NOT BRLCAD_ENABLE_TCL)

  execute_process(COMMAND ${CMAKE_COMMAND} ${BEXT_SOURCE_DIR}
    ${BEXT_CMAKE_OPTS}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${BEXT_INSTALL_DIR}
    WORKING_DIRECTORY ${BEXT_BUILD_DIR}
    )

  if (CMAKE_CONFIGURATION_TYPES)
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BEXT_BUILD_DIR} --parallel 8 --config ${CMAKE_BUILD_TYPE} WORKING_DIRECTORY ${BEXT_BUILD_DIR})
  else (CMAKE_CONFIGURATION_TYPES)
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BEXT_BUILD_DIR} --parallel 8 WORKING_DIRECTORY ${BEXT_BUILD_DIR})
  endif (CMAKE_CONFIGURATION_TYPES)

  set(BRLCAD_EXT_DIR ${BEXT_INSTALL_DIR}/bext_output CACHE PATH "Local bext install" FORCE)
  set(BRLCAD_EXT_INSTALL_DIR ${BRLCAD_EXT_DIR}/install CACHE PATH "Local bext install" FORCE)
  set(BRLCAD_EXT_NOINSTALL_DIR ${BRLCAD_EXT_DIR}/noinstall CACHE PATH "Local bext install" FORCE)
endfunction(bext_setup)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
