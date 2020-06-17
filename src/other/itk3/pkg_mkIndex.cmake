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

string(REPLACE "\\" "" TF_DIR ${TF_DIR})
string(REPLACE "\\" "" INST_DIR ${INST_DIR})
string(REPLACE "\\" "" WORKING_PKGFILE ${WORKING_PKGFILE})
string(REPLACE "\\" "" INSTALL_PKGFILE ${INSTALL_PKGFILE})

file(WRITE "${WORKING_PKGFILE}" "if {![package vsatisfies [package provide Tcl] 8.6]} return\n")
file(APPEND "${WORKING_PKGFILE}" "if {[string length [package provide Itcl]] && ![package vsatisfies [package provide Itcl] 3.4]} return\n")
file(APPEND "${WORKING_PKGFILE}" "package ifneeded itk ${pkgversion} [list load [file join $dir \"${TF_DIR}\" ${TF_NAME}] Itk]\n")
file(APPEND "${WORKING_PKGFILE}" "package ifneeded Itk ${pkgversion} [list load [file join $dir \"${TF_DIR}\" ${TF_NAME}] Itk]\n")


file(WRITE "${INSTALL_PKGFILE}" "if {![package vsatisfies [package provide Tcl] 8.6]} return\n")
file(APPEND "${INSTALL_PKGFILE}" "if {[string length [package provide Itcl]] && ![package vsatisfies [package provide Itcl] 4.1]} return\n")
file(APPEND "${INSTALL_PKGFILE}" "package ifneeded itk ${pkgversion} [list load [file join $dir .. .. \"${INST_DIR}\" ${TF_NAME}] Itk]\n")
file(APPEND "${INSTALL_PKGFILE}" "package ifneeded Itk ${pkgversion} [list load [file join $dir .. .. \"${INST_DIR}\" ${TF_NAME}] Itk]\n")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
