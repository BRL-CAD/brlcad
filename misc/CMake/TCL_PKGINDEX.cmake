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
function(TCL_PKGINDEX target pkgname pkgversion)

  # Identify the shared library suffix to be used
  set(lname ${CMAKE_SHARED_LIBRARY_PREFIX}${target}${CMAKE_SHARED_LIBRARY_SUFFIX})

  # Create a "working" pkgIndex.tcl file that will allow
  # the package to work from the build directory
  set(lld_install "${LIB_DIR}")
  if(MSVC)
    set(lld_install "${BIN_DIR}")
  endif(MSVC)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    set(lld_build "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(bpkgindex "${CMAKE_BINARY_DIR}/lib/${pkgname}${pkgversion}/pkgIndex.tcl")
    file(WRITE "${bpkgindex}" "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir \"${lld_build}\" ${lname}] ${pkgname}]")
    DISTCLEAN("${CMAKE_BINARY_DIR}/lib/${pkgname}${pkgversion}")
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      set(bpkgindex "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}/${pkgname}${pkgversion}/pkgIndex.tcl")
      set(lld_build "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
      if(MSVC)
	set(lld_build "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
      endif(MSVC)
      file(WRITE "${bpkgindex}" "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir \"${lld_build}\" ${lname}] ${pkgname}]")
      DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}/${pkgname}${pkgversion})
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)

  # Create the file Tcl will use once installed
  set(ipkgindex "${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl")
  file(WRITE "${ipkgindex}" "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir .. .. \"${lld_install}\" ${lname}] ${pkgname}]")
  install(FILES "${ipkgindex}" DESTINATION lib/${pkgname}${pkgversion})
  DISTCLEAN("${ipkgindex}")

endfunction(TCL_PKGINDEX)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
