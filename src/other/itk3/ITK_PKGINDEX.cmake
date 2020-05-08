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
function(ITK_PKGINDEX target pkgname pkgversion)

  set(INST_DIR "${LIB_DIR}")
  if(MSVC)
    set(INST_DIR "${BIN_DIR}")
  endif(MSVC)

  set(WORKING_PKGFILE "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${LIB_DIR}/${pkgname}${pkgversion}/pkgIndex.tcl")
  set(INSTALL_PKGFILE "${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl")

  add_custom_command(OUTPUT ${WORKING_PKGFILE} ${INSTALL_PKGFILE}
    COMMAND ${CMAKE_COMMAND} -DWORKING_PKGFILE="${WORKING_PKGFILE}" -DINSTALL_PKGFILE="${INSTALL_PKGFILE}" -DTF_NAME="$<TARGET_FILE_NAME:${target}>" -DTF_DIR="$<TARGET_FILE_DIR:${target}>" -Dpkgname="${pkgname}" -Dpkgversion="${pkgversion}" -DINST_DIR="${INST_DIR}" -P ${CMAKE_CURRENT_SOURCE_DIR}/pkg_mkIndex.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pkg_mkIndex.cmake
    )
  add_custom_target(${pkgname}_pkgIndex ALL DEPENDS ${WORKING_PKGFILE} ${INSTALL_PKGFILE})

  install(FILES ${INSTALL_PKGFILE} DESTINATION ${LIB_DIR}/${pkgname}${pkgversion})

  DISTCLEAN("${WORKING_PKGFILE}")
  DISTCLEAN("${INSTALL_PKGFILE}")

endfunction(ITK_PKGINDEX)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
