#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2020 United States Government as represented by
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

project(TERMLIB)

cmake_minimum_required(VERSION 3.1.3)

if(NOT MSVC)

  if(NOT BIN_DIR)
    set(BIN_DIR bin)
  endif(NOT BIN_DIR)

  if(NOT LIB_DIR)
    set(LIB_DIR lib)
  endif(NOT LIB_DIR)


  if(NOT DATA_DIR)
    set(DATA_DIR etc)
  endif(NOT DATA_DIR)

  set(LIBTERM_SOURCES
    termcap.c
    tgoto.c
    tputs.c
    )

  find_program(BHEXDUMP_EXECUTABLE bhexdump)
  if (NOT EXISTS "${BHEXDUMP_EXECUTABLE}")
    message(FATAL_ERROR "bhexdump not found")
  endif (NOT EXISTS "${BHEXDUMP_EXECUTABLE}")

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/termcap_src.h
    COMMAND ${BHEXDUMP_EXECUTABLE} -i ${CMAKE_CURRENT_SOURCE_DIR}/termcap.src > ${CMAKE_CURRENT_BINARY_DIR}/termcap_src.h
    DEPENDS ${BHEXDUMP_EXECUTABLE_TARGET} ${CMAKE_CURRENT_SOURCE_DIR}/termcap.src
    )
  add_custom_target(termcap_src ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/termcap_src.h)

  add_definitions(-DCM_N -DCM_GT -DCM_B -DCM_D
    -DB_TERMCAP=\"${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/termcap\"
    )

  add_library(termlib SHARED ${LIBTERM_SOURCES})
  set_target_properties(termlib PROPERTIES VERSION 1.7.1 SOVERSION 1)
  install(TARGETS termlib
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})

  if(BUILD_STATIC_LIBS)
    add_library(termlib-static STATIC ${LIBTERM_SOURCES})
    install(TARGETS termlib-static
      RUNTIME DESTINATION ${BIN_DIR}
      LIBRARY DESTINATION ${LIB_DIR}
      ARCHIVE DESTINATION ${LIB_DIR})
    set_target_properties(termlib-static PROPERTIES OUTPUT_NAME "termlib")
  endif(BUILD_STATIC_LIBS)

  if (NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/termcap.src ${CMAKE_BINARY_DIR}/${DATA_DIR}/termcap COPYONLY)
    install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/termcap DESTINATION ${DATA_DIR})
  else (NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/termcap.src ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/${DATA_DIR}/etc/termcap COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    install(FILES ${CMAKE_BINARY_DIR}/\${BUILD_TYPE}/${DATA_DIR}/termcap DESTINATION ${DATA_DIR})
  endif (NOT CMAKE_CONFIGURATION_TYPES)

endif(NOT MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
