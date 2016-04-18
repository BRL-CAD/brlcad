#           B R L C A D _ E N V _ I N I T . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2016 United States Government as represented by
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

#---------------------------------------------------------------------
# Allow the BRLCAD_ROOT environment variable to set CMAKE_INSTALL_PREFIX
# but be noisy about it.  This is generally not a good idea.
find_program(SLEEP_EXEC sleep)
mark_as_advanced(SLEEP_EXEC)
set(BRLCAD_ROOT "$ENV{BRLCAD_ROOT}")
if(BRLCAD_ROOT)
  if(NOT BRLCAD_ROOT_OVERRIDE)
    message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nBRLCAD_ROOT should only be used to override an install directory at runtime. BRLCAD_ROOT is presently set to \"${BRLCAD_ROOT}\"  It is *highly* recommended that BRLCAD_ROOT be unset and not used.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
    if(CMAKE_INSTALL_PREFIX AND NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
      if(${BRLCAD_ROOT} STREQUAL "${CMAKE_INSTALL_PREFIX}")
	message("BRLCAD_ROOT is not necessary and may cause unexpected behavior")
      else(${BRLCAD_ROOT} STREQUAL "${CMAKE_INSTALL_PREFIX}")
	message(FATAL_ERROR "BRLCAD_ROOT environment variable conflicts with CMAKE_INSTALL_PREFIX")
      endif(${BRLCAD_ROOT} STREQUAL "${CMAKE_INSTALL_PREFIX}")
    endif(CMAKE_INSTALL_PREFIX AND NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(SLEEP_EXEC)
      execute_process(COMMAND ${SLEEP_EXEC} 2)
    endif(SLEEP_EXEC)
  endif(NOT BRLCAD_ROOT_OVERRIDE)
endif(BRLCAD_ROOT)
if(NOT BRLCAD_ROOT_OVERRIDE)
  if(BRLCAD_ROOT STREQUAL "/usr" OR "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
    message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nIt is STRONGLY recommended that you DO NOT install BRL-CAD into /usr as BRL-CAD provides several libraries that may conflict with other libraries (e.g. librt, libbu, libbn) on certain system configurations.\nSince our libraries predate all those that we're known to conflict with and are at the very core of our geometry services and project heritage, we have no plans to change the names of our libraries at this time.\nINSTALLING INTO /usr CAN MAKE A SYSTEM COMPLETELY UNUSABLE.  If you choose to continue installing into /usr, you do so entirely at your own risk.  You have been warned.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
    if(SLEEP_EXEC)
      message("Pausing 15 seconds...")
      execute_process(COMMAND ${SLEEP_EXEC} 15)
    endif(SLEEP_EXEC)
    if(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
      message(FATAL_ERROR "If you wish to proceed using /usr as your prefix, define BRLCAD_ROOT_OVERRIDE=1 for CMake")
    endif(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
  endif(BRLCAD_ROOT STREQUAL "/usr" OR "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
endif(NOT BRLCAD_ROOT_OVERRIDE)

# While we're at it, complain if BRLCAD_DATA is set
set(BRLCAD_DATA_ENV "$ENV{BRLCAD_DATA}")
if(BRLCAD_DATA_ENV)
  message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nBRLCAD_DATA should only be used to override an install directory at runtime. BRLCAD_DATA is presently set to \"${BRLCAD_DATA_ENV}\"  It is *highly* recommended that BRLCAD_DATA be unset and not used.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
  if(SLEEP_EXEC)
    execute_process(COMMAND ${SLEEP_EXEC} 2)
  endif(SLEEP_EXEC)
endif(BRLCAD_DATA_ENV)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
