#
# - Find lemon executable and provides macros to generate custom build rules
# The module defines the following variables
#
#  LEMON_EXECUTABLE - path to the lemon program
#  LEMON_TEMPLATE - location of the lemon template file

#=============================================================================
#                 F I N D L E M O N . C M A K E
#
# Originally based off of FindBISON.cmake from Kitware's CMake distribution
#
# Copyright (c) 2010-2013 United States Government as represented by
#                the U.S. Army Research Laboratory.
# Copyright 2009 Kitware, Inc.
# Copyright 2006 Tristan Carel
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

find_program(LEMON_EXECUTABLE lemon DOC "path to the lemon executable")
mark_as_advanced(LEMON_EXECUTABLE)

if(LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)
  get_filename_component(lemon_path ${LEMON_EXECUTABLE} PATH)
  if(lemon_path)
    set(LEMON_TEMPLATE "")
    if(EXISTS ${lemon_path}/lempar.c)
      set(LEMON_TEMPLATE "${lemon_path}/lempar.c")
    endif(EXISTS ${lemon_path}/lempar.c)
    if(EXISTS /usr/share/lemon/lempar.c)
      set(LEMON_TEMPLATE "/usr/share/lemon/lempar.c")
    endif(EXISTS /usr/share/lemon/lempar.c)
  endif(lemon_path)
endif(LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LEMON DEFAULT_MSG LEMON_EXECUTABLE LEMON_TEMPLATE)
mark_as_advanced(LEMON_TEMPLATE)

#============================================================
# FindLEMON.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
