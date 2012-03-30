#               B R L C A D _ U T I L . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
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
#-----------------------------------------------------------------------------
# Pretty-printing macro that generates a box around a string and prints the
# resulting message.
macro(BOX_PRINT input_string border_string)
  string(LENGTH ${input_string} MESSAGE_LENGTH)
  string(LENGTH ${border_string} SEPARATOR_STRING_LENGTH)
  while(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
    set(SEPARATOR_STRING "${SEPARATOR_STRING}${border_string}")
    string(LENGTH ${SEPARATOR_STRING} SEPARATOR_STRING_LENGTH)
  endwhile(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
  message("${SEPARATOR_STRING}")
  message("${input_string}")
  message("${SEPARATOR_STRING}")
endmacro()

#-----------------------------------------------------------------------------
include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)

macro(CPP_WARNINGS srcslist)
  # We need to specify specific flags for c++ files - their warnings are
  # not usually used to hault the build, althogh BRLCAD_ENABLE_CXX_STRICT
  # can be set to on to achieve that
  if(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
    foreach(srcfile ${${srcslist}})
      if(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
	if(BRLCAD_ENABLE_COMPILER_WARNINGS)
	  if(NOERROR_FLAG)
	    get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	    set_source_files_properties(${srcfile} COMPILE_FLAGS "-Wno-error ${previous_flags}")
	  endif(NOERROR_FLAG)
	else(BRLCAD_ENABLE_COMPILER_WARNINGS)
	  get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	  set_source_files_properties(${srcfile} COMPILE_FLAGS "-w ${previous_flags}")
	endif(BRLCAD_ENABLE_COMPILER_WARNINGS)
      endif(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
    endforeach(srcfile ${${srcslist}})
  endif(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
endmacro(CPP_WARNINGS)

#-----------------------------------------------------------------------------
# It is sometimes convenient to be able to supply both a filename and a 
# variable name containing a list of files to a single macro.
# This routine handles both forms of input - a separate variable is specified
# that contains the name of the variable holding the list, and in the case
# where there is no such variable one is prepared.  This way, subsequent
# macro logic need only deal with a variable holding a list, whatever the
# original form of the input.
macro(NORMALIZE_FILE_LIST inlist targetvar)
 set(havevarname 0)
  foreach(maybefilename ${inlist})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${maybefilename})
      set(havevarname 1)
    endif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${maybefilename})
  endforeach(maybefilename ${${targetvar}})
  if(NOT havevarname)
    set(FILELIST "${inlist}")
    set(${targetvar} "FILELIST")
  else(NOT havevarname)
    set(${targetvar} "${inlist}")
  endif(NOT havevarname)
endmacro(NORMALIZE_FILE_LIST)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
