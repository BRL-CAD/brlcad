#=============================================================================
#
# Copyright (c) 2010-2025 United States Government as represented by
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
get_filename_component(TFD "${TF_DIR}" REALPATH)

# file(WRITE) is digesting the paths and producing incorrect directories
# when there are spaces in paths.  To avoid this, use get_filename_component
# to "pre-digest" the paths and then correct any  "/ " patterns introduced.
get_filename_component(WFD "${WORKING_PKGFILE}" DIRECTORY)
get_filename_component(WFN "${WORKING_PKGFILE}" NAME)
string(REPLACE "/ " " " WFD "${WFD}")
file(
  WRITE
  "${WFD}/${WFN}"
  "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir \"${TFD}\" ${TF_NAME}] ${pkgname}]"
)

# file(WRITE) is digesting the paths and producing incorrect directories
# when there are spaces in paths.  To avoid this, use get_filename_component
# to "pre-digest" the paths and then correct any  "/ " patterns introduced.
get_filename_component(WFD "${INSTALL_PKGFILE}" DIRECTORY)
get_filename_component(WFN "${INSTALL_PKGFILE}" NAME)
string(REPLACE "/ " " " WFD "${WFD}")
file(
  WRITE
  "${WFD}/${WFN}"
  "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir .. .. \"${INST_DIR}\" ${TF_NAME}] ${pkgname}]"
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
