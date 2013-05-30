#
# - Find perplex executable and provides macros to generate custom build rules
# The module defines the following variables
#
#  PERPLEX_EXECUTABLE - path to the perplex program
#  PERPLEX_TEMPLATE - location of the perplex template file

#=============================================================================
#                 F I N D P E R P L E X . C M A K E
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

find_program(PERPLEX_EXECUTABLE perplex DOC "path to the perplex executable")
mark_as_advanced(PERPLEX_EXECUTABLE)

if(PERPLEX_EXECUTABLE AND NOT PERPLEX_TEMPLATE)
  get_filename_component(perplex_path ${PERPLEX_EXECUTABLE} PATH)
  if(perplex_path)
    set(PERPLEX_TEMPLATE "")
    if(EXISTS ${perplex_path}/../share/perplex/perplex_template.c)
      get_filename_component(perplex_template_path "${perplex_path}/../share/perplex/perplex_template.c" ABSOLUTE)
      set(PERPLEX_TEMPLATE "${perplex_template_path}")
    endif(EXISTS ${perplex_path}/../share/perplex/perplex_template.c)
    if(EXISTS ${perplex_path}/../share/perplex_template.c AND NOT PERPLEX_TEMPLATE)
      get_filename_component(perplex_template_path "${perplex_path}/../share/perplex_template.c" ABSOLUTE)
      set(PERPLEX_TEMPLATE "${perplex_template_path}")
    endif(EXISTS ${perplex_path}/../share/perplex_template.c AND NOT PERPLEX_TEMPLATE)
  endif(perplex_path)
  if(EXISTS /usr/share/perplex/perplex_template.c AND NOT PERPLEX_TEMPLATE)
    set(PERPLEX_TEMPLATE "/usr/share/perplex/perplex_template.c")
  endif(EXISTS /usr/share/perplex/perplex_template.c AND NOT PERPLEX_TEMPLATE)
endif(PERPLEX_EXECUTABLE AND NOT PERPLEX_TEMPLATE)
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PERPLEX DEFAULT_MSG PERPLEX_EXECUTABLE PERPLEX_TEMPLATE)
mark_as_advanced(PERPLEX_TEMPLATE)

#============================================================
# FindPERPLEX.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
