#             F I N D S H E L L D E P S . C M A K E
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
# - Find Programs needed to run .sh scripts.

# The following variables are set:
#
# SH_EXEC 
# CP_EXEC
# MV_EXEC
# RM_EXEC

FIND_PROGRAM(SH_EXEC NAMES sh dash bash DOC "path to shell executable")
FIND_PROGRAM(MV_EXEC NAMES mv DOC "path to move executable")
FIND_PROGRAM(CP_EXEC NAMES cp DOC "path to copy executable")
FIND_PROGRAM(RM_EXEC NAMES rm DOC "path to remove executable")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SHELL_SUPPORTED DEFAULT_MSG SH_EXEC CP_EXEC MV_EXEC RM_EXEC)
MARK_AS_ADVANCED(SH_EXEC)
MARK_AS_ADVANCED(MV_EXEC)
MARK_AS_ADVANCED(CP_EXEC)
MARK_AS_ADVANCED(RM_EXEC)

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
