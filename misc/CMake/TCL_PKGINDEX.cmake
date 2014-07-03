#=============================================================================
#
# Copyright (c) 2010-2014 United States Government as represented by
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
macro(TCL_PKGINDEX target pkgname pkgversion)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    set(loadable_libdir_build "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(loadable_libdir_install "${LIB_DIR}")
	 if(CMAKE_BUILD_TYPE)
      get_target_property(target_LIBLOCATION ${target} LOCATION_${CMAKE_BUILD_TYPE})
    else(CMAKE_BUILD_TYPE)
      get_target_property(target_LIBLOCATION ${target} LOCATION)
	 endif(CMAKE_BUILD_TYPE)
    get_filename_component(target_LIBNAME ${target_LIBLOCATION} NAME)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir .. .. \"${loadable_libdir_install}\" ${target_LIBNAME}] ${pkgname}]")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${pkgname}${pkgversion})
    file(WRITE ${CMAKE_BINARY_DIR}/lib/${pkgname}${pkgversion}/pkgIndex.tcl "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir \"${loadable_libdir_build}\" ${target_LIBNAME}] ${pkgname}]")
    DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl)
    DISTCLEAN(${CMAKE_BINARY_DIR}/lib/${pkgname}${pkgversion})
  else(NOT CMAKE_CONFIGURATION_TYPES)
    if(MSVC)
      set(loadable_libdir_install "${BIN_DIR}")
    else(MSVC)
      set(loadable_libdir_install "${LIB_DIR}")
    endif(MSVC)
    DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      if(MSVC)
	set(loadable_libdir_build "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
      else(MSVC)
	set(loadable_libdir_build "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
      endif(MSVC)
      get_target_property(target_LIBLOCATION ${target} LOCATION_${CFG_TYPE_UPPER})
      get_filename_component(target_LIBNAME ${target_LIBLOCATION} NAME)
      file(WRITE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}/${pkgname}${pkgversion}/pkgIndex.tcl "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir \"${loadable_libdir_build}\" ${target_LIBNAME}] ${pkgname}]")
      DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}/${pkgname}${pkgversion})
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${pkgname} ${pkgversion} [list load [file join $dir .. .. \"${loadable_libdir_install}\" ${target_LIBNAME}] ${pkgname}]")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${pkgname}${pkgversion})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endmacro(TCL_PKGINDEX)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
