#=============================================================================
#
# Copyright (c) 2010-2020 United States Government as represented by
#                the U.S. Army Research Laboratory.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * The names of the authors may not be used to endorse or promote
#   products derived from this software without specific prior written
#   permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

#============================================================
# TCL_PKGINDEX
#============================================================

# Custom patch utility to replace the build directory path with the install
# directory path in text files - make sure CMAKE_BINARY_DIR and
# CMAKE_INSTALL_PREFIX are finalized before generating this file!
configure_file(${CMAKE_SOURCE_DIR}/CMake/buildpath_replace.cxx.in ${CMAKE_CURRENT_BINARY_DIR}/buildpath_replace.cxx)
add_executable(buildpath_replace ${CMAKE_CURRENT_BINARY_DIR}/buildpath_replace.cxx)

function(TCL_PKGINDEX target pkgname pkgversion)

  set(PKGFILE ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${LIB_DIR}/${pkgname}${pkgversion}/pkgIndex.tcl)

  add_custom_command(OUTPUT ${PKGFILE}
    COMMAND ${CMAKE_COMMAND} -DPKGFILE="${PKGFILE}" -DTF_NAME="$<TARGET_FILE_NAME:${target}>" -DTF_DIR="$<TARGET_FILE_DIR:${target}>" -DLIB_DIR="${LIB_DIR}" -Dpkgname="${pkgname}" -Dpkgversion="${pkgversion}" -P ${CMAKE_SOURCE_DIR}/CMake/scripts/tcl_mkindex.cmake
    )
  add_custom_target(${pkgname}_pkgIndex ALL DEPENDS ${PKGFILE})

  install(FILES ${PKGFILE} DESTINATION ${LIB_DIR}/${pkgname}${pkgversion})
  install(CODE "execute_process(COMMAND ${CMAKE_BINARY_DIR}/${BIN_DIR}/buildpath_replace${CMAKE_EXECUTABLE_SUFFIX} \"\${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${pkgname}${pkgversion}/pkgIndex.tcl\")")

endfunction(TCL_PKGINDEX)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
