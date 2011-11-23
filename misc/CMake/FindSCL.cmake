#                   F I N D S C L . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
#
###
# - Find STEP Class Library binaries and libraries
#
# The following variables are set:
#
# SCL_INCLUDE_DIR
# SCL_EXPRESS_EXECUTABLE
# SCL_SYMLINK_EXECUTABLE
# SCL_EXPPP_EXECUTABLE
# SCL_FEDEX_OS_EXECUTABLE
# SCL_FEDEX_IDL_EXECUTABLE
# SCL_FEDEX_PLUS_EXECUTABLE
# SCL_EXPPP_LIB
# SCL_CORE_LIB
# SCL_UTILS_LIB
# SCL_DAI_LIB
# SCL_EDITOR_LIB

FIND_PROGRAM(SCL_EXPRESS_EXECUTABLE express  DOC "path to the SCL express executable")
FIND_PROGRAM(SCL_SYMLINK_EXECUTABLE symlink DOC "path to the SCL symlink executable")
FIND_PROGRAM(SCL_EXPPP_EXECUTABLE exppp DOC "path to the SCL exppp executable")
FIND_PROGRAM(SCL_FEDEX_OS_EXECUTABLE fedex_os DOC "path to the SCL fedex_os executable")
FIND_PROGRAM(SCL_FEDEX_IDL_EXECUTABLE fedex_idl DOC "path to the SCL fedex_idl executable")
FIND_PROGRAM(SCL_FEDEX_PLUS_EXECUTABLE fedex_plus  DOC "path to the SCL fedex_plus executable")

FIND_LIBRARY(SCL_EXPPP_LIB NAMES exppp)
FIND_LIBRARY(SCL_CORE_LIB NAMES stepcore)
FIND_LIBRARY(SCL_DAI_LIB NAMES stepdai)
FIND_LIBRARY(SCL_UTILS_LIB NAMES steputils)
FIND_LIBRARY(SCL_EDITOR_LIB NAMES stepeditor)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SCL DEFAULT_MSG SCL_CORE_LIB SCL_DAI_LIB SCL_UTILS_LIB SCL_FEDEX_PLUS_EXECUTABLE SCL_EXPRESS_EXECUTABLE)

MARK_AS_ADVANCED(SCL_CORE_LIB SCL_DAI_LIB SCL_EDITOR_LIB SCL_EXPPP_LIB SCL_UTILS_LIB)
MARK_AS_ADVANCED(SCL_EXPPP_EXECUTABLE SCL_EXPRESS_EXECUTABLE SCL_FEDEX_IDL_EXECUTABLE SCL_FEDEX_OS_EXECUTABLE SCL_FEDEX_PLUS_EXECUTABLE SCL_SYMLINK_EXECUTABLE)

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
