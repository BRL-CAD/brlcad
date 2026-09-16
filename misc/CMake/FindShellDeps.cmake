#             F I N D S H E L L D E P S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2026 United States Government as represented by
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

set(_SHELL_PROGRAM_HINTS)
set(_SHELL_EXEC_HINTS)
if(WIN32)
  # Git for Windows is a common source of POSIX command-line tools on Windows.
  # Search its default install locations when the tools are not already on PATH.
  # Prefer Git's bin/sh.exe shim for SH_EXEC: when invoked directly it sets up a
  # usable PATH for the rest of Git's POSIX tools.
  list(
    APPEND
    _SHELL_EXEC_HINTS
    "$ENV{ProgramFiles}/Git/bin"
    "$ENV{ProgramFiles\(x86\)}/Git/bin"
  )
  list(APPEND _SHELL_PROGRAM_HINTS ${_SHELL_EXEC_HINTS})
  list(
    APPEND
    _SHELL_PROGRAM_HINTS
    "$ENV{ProgramFiles}/Git/usr/bin"
    "$ENV{ProgramFiles\(x86\)}/Git/usr/bin"
  )
endif(WIN32)

find_program(SH_EXEC NAMES sh dash bash HINTS ${_SHELL_EXEC_HINTS} ${_SHELL_PROGRAM_HINTS} DOC "path to shell executable")
find_program(MV_EXEC NAMES mv HINTS ${_SHELL_PROGRAM_HINTS} DOC "path to move executable")
find_program(CP_EXEC NAMES cp HINTS ${_SHELL_PROGRAM_HINTS} DOC "path to copy executable")
find_program(RM_EXEC NAMES rm HINTS ${_SHELL_PROGRAM_HINTS} DOC "path to remove executable")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  SHELL_SUPPORTED
  DEFAULT_MSG
  SH_EXEC
  CP_EXEC
  MV_EXEC
  RM_EXEC
)
mark_as_advanced(SH_EXEC)
mark_as_advanced(MV_EXEC)
mark_as_advanced(CP_EXEC)
mark_as_advanced(RM_EXEC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
