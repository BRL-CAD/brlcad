# Copyright (c) 2016 United States Government as represented by
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

# *******************************************************************
# ***                        CMakeLists.txt                       ***
# *******************************************************************

# External project management - see
# https://cmake.org/cmake/help/latest/module/ExternalProject.html

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

# set CMake project name
project(SP)

# Standard path definitions
include(${CMAKE_SOURCE_DIR}/CMake/Path_Setup.cmake)

# Use this variable to key both CMake messages and tools like rpath_replace
if(NOT DEFINED EXTPROJ_VERBOSE)
  set(EXTPROJ_VERBOSE 0)
endif(NOT DEFINED EXTPROJ_VERBOSE)

# Custom patch utility for external RPath preparation
configure_file(${CMAKE_SOURCE_DIR}/CMake/rpath_replace.cxx.in ${CMAKE_BINARY_DIR}/rpath_replace.cxx @ONLY)
add_executable(rpath_replace ${CMAKE_BINARY_DIR}/rpath_replace.cxx)

# TODO - hardcode this for easy testing - should go away once we're ready
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/../install)

# Load the primary functionality for external project management
include(${CMAKE_SOURCE_DIR}/CMake/RPath_Setup.cmake)
cmake_set_rpath()
include(ExternalProject)
# Note- make sure CMAKE_BINARY_DIR and CMAKE_INSTALL_PREFIX are set before including this file
include(${CMAKE_SOURCE_DIR}/CMake/ExternalProject_Target.cmake)

# With MSVC, some of the subbuilds are going to need the vcvars bat file
# to set up the environment.
if(MSVC)
  get_filename_component(COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
  get_filename_component(COMPILER_ROOT "${COMPILER_DIR}" NAME)
  set(VCVARS_BAT "${COMPILER_DIR}/vcvars${COMPILER_ROOT}.bat")
  if(NOT EXISTS "${VCVARS_BAT}")
    message(FATAL_ERROR "Could not find vcvars${COMPILER_ROOT}.bat file in ${COMPILER_DIR}")
  endif(NOT EXISTS "${VCVARS_BAT}")
endif(MSVC)

###############################################################################
#                                 Zlib
###############################################################################
ExternalProject_Add(ZLIB_BLD
  URL ${CMAKE_SOURCE_DIR}/external/zlib-1.2.11.tar.gz
  URL_HASH MD5=1c9f62f0778697a09d36121ead88e08e
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
  )
ExternalProject_Target(zlib ZLIB_BLD
  OUTPUT_FILE libz${CMAKE_SHARED_LIBRARY_SUFFIX}.1.2.11
  STATIC_OUTPUT_FILE libz${CMAKE_STATIC_LIBRARY_SUFFIX}
  SYMLINKS "libz${CMAKE_SHARED_LIBRARY_SUFFIX};libz${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
  LINK_TARGET "libz${CMAKE_SHARED_LIBRARY_SUFFIX}"
  )
ExternalProject_ByProducts(ZLIB_BLD ${INCLUDE_DIR}
  zconf.h
  zlib.h
  )

# Temporarily stub in the library variable setting here - normally this will be
# defined by the ThirdParty logic but we need to test that the import library
# works, particularly with regards to build dependencies since the zlib target
# itself is not a build target, but has to require that the ZLIB_BLD target
# execute before anything that needs it is built.
set(ZLIB_LIBRARY zlib)


###############################################################################
#                                 libpng 
#
# Have find_package check for a local libz before trying the system search (in
# case we are building our own zlib) by defining the ZLIB_ROOT prefix for
# find_package
###############################################################################
ExternalProject_Add(PNG_BLD
  URL ${CMAKE_SOURCE_DIR}/external/libpng-1.6.35.tar.gz
  URL_HASH MD5=d94d9587c421ac42316b6ab8f64f1b85
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_INSTALL_LIBDIR=${LIB_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DZLIB_ROOT=${CMAKE_BINARY_DIR}
  DEPENDS ${ZLIB_LIBRARY}
  )
ExternalProject_Target(png PNG_BLD
  OUTPUT_FILE libpng16${CMAKE_SHARED_LIBRARY_SUFFIX}.16.35.0
  STATIC_OUTPUT_FILE libpng16${CMAKE_STATIC_LIBRARY_SUFFIX}
  SYMLINKS "libpng${CMAKE_SHARED_LIBRARY_SUFFIX};libpng16${CMAKE_SHARED_LIBRARY_SUFFIX};libpng16${CMAKE_SHARED_LIBRARY_SUFFIX}.16;libpng${CMAKE_STATIC_LIBRARY_SUFFIX}"
  LINK_TARGET "libpng${CMAKE_SHARED_LIBRARY_SUFFIX}"
  STATIC_LINK_TARGET "libpng${CMAKE_STATIC_LIBRARY_SUFFIX}"
  RPATH
  )
ExternalProject_ByProducts(PNG_BLD ${INCLUDE_DIR}
  png.h
  pngconf.h
  pnglibconf.h
  libpng16/png.h
  libpng16/pngconf.h
  libpng16/pnglibconf.h
  )

# Temporarily stub in the library variable setting here
set(PNG_LIBRARY png)


###############################################################################
#                                  freetype
#
# Note: Having find_package check for local libz and libpng versions before
# trying the system search by defining the ZLIB_ROOT and PNG_ROOT prefixes for
# find_package
###############################################################################
ExternalProject_Add(FREETYPE_BLD
  URL ${CMAKE_SOURCE_DIR}/external/freetype-2.9.tar.bz2
  URL_HASH MD5=513c403c110016fdc7e537216a642b1d
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=ON -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DPNG_ROOT=${CMAKE_BINARY_DIR} -DZLIB_ROOT=${CMAKE_BINARY_DIR}
  DEPENDS ${ZLIB_LIBRARY} ${PNG_LIBRARY}
  )
ExternalProject_Target(freetype FREETYPE_BLD
  OUTPUT_FILE libfreetype${CMAKE_SHARED_LIBRARY_SUFFIX}.2.9.0
  SYMLINKS "libfreetype${CMAKE_SHARED_LIBRARY_SUFFIX};libfreetype${CMAKE_SHARED_LIBRARY_SUFFIX}.2.9"
  LINK_TARGET "libfreetype${CMAKE_SHARED_LIBRARY_SUFFIX}"
  RPATH
  )
ExternalProject_ByProducts(FREETYPE_BLD ${INCLUDE_DIR}
  freetype2/ft2build.h
  freetype2/freetype/config/ftheader.h
  freetype2/freetype/config/ftmodule.h
  freetype2/freetype/config/ftstdlib.h
  freetype2/freetype/ftlcdfil.h
  freetype2/freetype/ftsynth.h
  freetype2/freetype/ftmm.h
  freetype2/freetype/ftsizes.h
  freetype2/freetype/freetype.h
  freetype2/freetype/tttables.h
  freetype2/freetype/ftotval.h
  freetype2/freetype/ftcache.h
  freetype2/freetype/tttags.h
  freetype2/freetype/ftbbox.h
  freetype2/freetype/fterrors.h
  freetype2/freetype/ftfntfmt.h
  freetype2/freetype/ftsystem.h
  freetype2/freetype/ftbdf.h
  freetype2/freetype/ftmoderr.h
  freetype2/freetype/ftimage.h
  freetype2/freetype/ftoutln.h
  freetype2/freetype/ftmodapi.h
  freetype2/freetype/fttrigon.h
  freetype2/freetype/ftrender.h
  freetype2/freetype/ftwinfnt.h
  freetype2/freetype/ftadvanc.h
  freetype2/freetype/ftchapters.h
  freetype2/freetype/ftcid.h
  freetype2/freetype/ftpfr.h
  freetype2/freetype/ftparams.h
  freetype2/freetype/fterrdef.h
  freetype2/freetype/ttnameid.h
  freetype2/freetype/ftstroke.h
  freetype2/freetype/ftbzip2.h
  freetype2/freetype/ftgxval.h
  freetype2/freetype/ftsnames.h
  freetype2/freetype/ftglyph.h
  freetype2/freetype/ftlist.h
  freetype2/freetype/ftdriver.h
  freetype2/freetype/fttypes.h
  freetype2/freetype/ftgasp.h
  freetype2/freetype/ftlzw.h
  freetype2/freetype/t1tables.h
  freetype2/freetype/ftincrem.h
  freetype2/freetype/ftgzip.h
  freetype2/freetype/ftbitmap.h
  freetype2/freetype/ftmac.h
  freetype2/freetype/config/ftconfig.h
  freetype2/freetype/config/ftoption.h
  )

###############################################################################
#                                  fontconfig
#
# Note: Since Tk uses this on some platforms, on those platforms we must also
# provide a local fontconfig if we want to use a local freetype
###############################################################################

if(NOT MSVC)
  if (TARGET FREETYPE_BLD)

    # fontconfig requires expat
    ExternalProject_Add(EXPAT_BLD
      URL ${CMAKE_SOURCE_DIR}/external/expat-2.2.6.tar.bz2
      URL_HASH MD5=ca047ae951b40020ac831c28859161b2
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_LIBDIR=${LIB_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
      )
    ExternalProject_Target(expat EXPAT_BLD
      OUTPUT_FILE libexpat${CMAKE_SHARED_LIBRARY_SUFFIX}.1.6.8
      SYMLINKS "libexpat${CMAKE_SHARED_LIBRARY_SUFFIX};libexpat${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
      LINK_TARGET "libexpat${CMAKE_SHARED_LIBRARY_SUFFIX}"
      RPATH
      )

    # fontconfig requires gperf
    set(GPERF_SRC_DIR "${CMAKE_BINARY_DIR}/GPERF_BLD-prefix/src/GPERF_BLD")
    set(GPERF_PATCH_FILES "${GPERF_SRC_DIR}/configure")
    ExternalProject_Add(GPERF_BLD
      URL ${CMAKE_SOURCE_DIR}/external/gperf-3.1.tar.gz
      URL_HASH MD5=9e251c0a618ad0824b51117d5d9db87e
      PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${GPERF_PATCH_FILES}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR}/pkgconfig ${GPERF_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR}
      BUILD_COMMAND make -j9
      BUILD_IN_SOURCE 1
      INSTALL_COMMAND make install
      )
    ExternalProject_Target(gperf GPERF_BLD EXEC
      OUTPUT_FILE gperf${CMAKE_EXECUTABLE_SUFFIX}
      )

    set(FONTCONFIG_SRC_DIR "${CMAKE_BINARY_DIR}/FONTCONFIG_BLD-prefix/src/FONTCONFIG_BLD")
    set(FONTCONFIG_PATCH_FILES "${FONTCONFIG_SRC_DIR}/configure")
    ExternalProject_Add(FONTCONFIG_BLD
      URL ${CMAKE_SOURCE_DIR}/external/fontconfig-2.12.3.tar.bz2
      URL_HASH MD5=872f757ec180e8c51918ff39705c40de
      PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${FONTCONFIG_PATCH_FILES}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E remove ${FONTCONFIG_SRC_DIR}/src/fcobjshash.gperf
      COMMAND ${CMAKE_COMMAND} -E remove ${FONTCONFIG_SRC_DIR}/src/fcobjshash.h
      COMMAND ${CMAKE_COMMAND} -E env GPERF=${CMAKE_BINARY_DIR}/${BIN_DIR}/gperf${CMAKE_EXECUTABLE_SUFFIX} PKG_CONFIG_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR}/pkgconfig FREETYPE_LIBS=${CMAKE_BINARY_DIR}/${LIB_DIR}/libfreetype${CMAKE_SHARED_LIBRARY_SUFFIX} FREETYPE_LIBS=${CMAKE_BINARY_DIR}/${LIB_DIR}/libfreetype${CMAKE_SHARED_LIBRARY_SUFFIX} FREETYPE_CFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/freetype2 LDFLAGS=-lfreetype ${FONTCONFIG_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR}
      BUILD_COMMAND make -j9
      BUILD_IN_SOURCE 1
      INSTALL_COMMAND make install
      DEPENDS FREETYPE_BLD EXPAT_BLD GPERF_BLD
      )
    ExternalProject_Target(fontconfig FONTCONFIG_BLD
      OUTPUT_FILE libfontconfig${CMAKE_SHARED_LIBRARY_SUFFIX}.1.9.3
      SYMLINKS "libfontconfig${CMAKE_SHARED_LIBRARY_SUFFIX};libfontconfig${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
      LINK_TARGET "libfontconfig${CMAKE_SHARED_LIBRARY_SUFFIX}"
      )
  endif (TARGET FREETYPE_BLD)
else(NOT MSVC)
  set(FONTCONFIG_LIBRARY "")
endif(NOT MSVC)

###############################################################################
#                                 Tcl
###############################################################################
set(TCL_SRC_DIR "${CMAKE_BINARY_DIR}/TCL_BLD-prefix/src/TCL_BLD")
if(NOT MSVC)
  set(TCL_PATCH_FILES "${TCL_SRC_DIR}/unix/configure" "${TCL_SRC_DIR}/macosx/configure" "${TCL_SRC_DIR}/unix/tcl.m4")
  ExternalProject_Add(TCL_BLD
    URL ${CMAKE_SOURCE_DIR}/external/tcl8.7a1-src.tar.gz
    URL_HASH MD5=27efa8d3d37efa3f06fc92f122f58f6f
    PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TCL_PATCH_FILES}
    CONFIGURE_COMMAND ${TCL_SRC_DIR}/unix/configure --prefix=${CMAKE_BINARY_DIR}
    BUILD_COMMAND make -j9
    INSTALL_COMMAND make install
    )
else(NOT MSVC)
  ExternalProject_Add(TCL_BLD
    URL ${CMAKE_SOURCE_DIR}/external/tcl8.7a1-src.tar.gz
    URL_HASH MD5=27efa8d3d37efa3f06fc92f122f58f6f
    CONFIGURE_COMMAND ""
    BINARY_DIR ${TCL_SRC_DIR}/win
    BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR}
    INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR}
    )
endif(NOT MSVC)
ExternalProject_Target(tcl TCL_BLD
  OUTPUT_FILE libtcl8.7${CMAKE_SHARED_LIBRARY_SUFFIX}
  STATIC_OUTPUT_FILE libtclstub8.7${CMAKE_STATIC_LIBRARY_SUFFIX}
  RPATH
  )
ExternalProject_Target(tclsh TCL_BLD
  OUTPUT_FILE tclsh8.7${CMAKE_EXECUTABLE_SUFFIX}
  RPATH EXEC
  )
ExternalProject_ByProducts(TCL_BLD ${LIB_DIR} FIXPATH
  tclConfig.sh
  tclooConfig.sh
  )
ExternalProject_ByProducts(TCL_BLD ${INCLUDE_DIR}
  tclDecls.h
  tcl.h
  tclOODecls.h
  tclOO.h
  tclPlatDecls.h
  tclTomMathDecls.h
  tclTomMath.h
  )

include(${CMAKE_SOURCE_DIR}/external/tcl_lib_byproducts.cmake)
ExternalProject_ByProducts(TCL_BLD ${LIB_DIR} ${TCL_LIB_BYPRODUCTS})


###############################################################################
#                                 Tk
###############################################################################
set(TK_SRC_DIR "${CMAKE_BINARY_DIR}/TK_BLD-prefix/src/TK_BLD")
if(NOT MSVC)
  set(TK_PATCH_FILES "${TK_SRC_DIR}/unix/configure" "${TK_SRC_DIR}/macosx/configure" "${TK_SRC_DIR}/unix/tcl.m4")
  ExternalProject_Add(TK_BLD
    URL ${CMAKE_SOURCE_DIR}/external/tk8.7a1-src.tar.gz
    URL_HASH MD5=099eb5571f4789194fa53d5590e7e623
    PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TK_PATCH_FILES}
    CONFIGURE_COMMAND ${TK_SRC_DIR}/unix/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR}
    BUILD_COMMAND make -j9
    INSTALL_COMMAND make install
    DEPENDS TCL_BLD ${FONTCONFIG_LIBRARY}
    )
else(NOT MSVC)
  ExternalProject_Add(TK_BLD
    URL ${CMAKE_SOURCE_DIR}/external/tk8.7a1-src.tar.gz
    URL_HASH MD5=099eb5571f4789194fa53d5590e7e623
    CONFIGURE_COMMAND ""
    BINARY_DIR ${TK_SRC_DIR}/win
    BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
    INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
    DEPENDS TCL_BLD
    )
endif(NOT MSVC)
ExternalProject_Target(tk TK_BLD
  OUTPUT_FILE libtk8.7${CMAKE_SHARED_LIBRARY_SUFFIX}
  STATIC_OUTPUT_FILE libtkstub8.7${CMAKE_STATIC_LIBRARY_SUFFIX}
  RPATH
  )
ExternalProject_Target(wish TK_BLD
  OUTPUT_FILE wish8.7${CMAKE_EXECUTABLE_SUFFIX}
  RPATH EXEC
  )

ExternalProject_ByProducts(TK_BLD ${LIB_DIR} FIXPATH
  tkConfig.sh
  )

ExternalProject_ByProducts(TK_BLD ${INCLUDE_DIR}
  tkDecls.h
  tk.h
  tkPlatDecls.h
  )

include(${CMAKE_SOURCE_DIR}/external/tk_lib_byproducts.cmake)
ExternalProject_ByProducts(TK_BLD ${LIB_DIR} ${TK_LIB_BYPRODUCTS})

###############################################################################
#                                 Itcl
###############################################################################
set(ITCL_SRC_DIR "${CMAKE_BINARY_DIR}/ITCL_BLD-prefix/src/ITCL_BLD")
if(NOT MSVC)
  set(ITCL_PATCH_FILES "${ITCL_SRC_DIR}/configure" "${ITCL_SRC_DIR}/tclconfig/tcl.m4")
  ExternalProject_Add(ITCL_BLD
    URL ${CMAKE_SOURCE_DIR}/external/itcl4.1.1.tar.gz
    URL_HASH MD5=198c9adb83550ffe87dbffaa5676ef54
    PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${ITCL_PATCH_FILES}
    CONFIGURE_COMMAND ${ITCL_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/lib
    BUILD_COMMAND make -j9
    INSTALL_COMMAND make install
    DEPENDS TCL_BLD
    )
else(NOT MSVC)
  ExternalProject_Add(ITCL_BLD
    URL ${CMAKE_SOURCE_DIR}/external/itcl4.1.1.tar.gz
    URL_HASH MD5=198c9adb83550ffe87dbffaa5676ef54
    CONFIGURE_COMMAND ""
    BINARY_DIR ${ITCL_SRC_DIR}/win
    BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
    INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
    DEPENDS TCL_BLD
    )
endif(NOT MSVC)
ExternalProject_Target(itcl ITCL_BLD
  IMPORT_PREFIX itcl4.1.1
  OUTPUT_FILE libitcl4.1.1${CMAKE_SHARED_LIBRARY_SUFFIX}
  STATIC_OUTPUT_FILE libitclstub4.1.1${CMAKE_STATIC_LIBRARY_SUFFIX}
  )

ExternalProject_ByProducts(ITCL_BLD ${INCLUDE_DIR}
  itcl2TclOO.h
  itclDecls.h
  itcl.h
  itclIntDecls.h
  itclInt.h
  itclMigrate2TclCore.h
  itclTclIntStubsFcn.h
  )

ExternalProject_ByProducts(ITCL_BLD ${LIB_DIR} FIXPATH
  itcl4.1.1/itclConfig.sh
  )

ExternalProject_ByProducts(ITCL_BLD ${LIB_DIR}
  itcl4.1.1/itclHullCmds.tcl
  itcl4.1.1/pkgIndex.tcl
  itcl4.1.1/itclWidget.tcl
  itcl4.1.1/itcl.tcl
  )

###############################################################################
#                                 Itk
###############################################################################
set(ITK_SRC_DIR "${CMAKE_BINARY_DIR}/ITK_BLD-prefix/src/ITK_BLD")
if(NOT MSVC)
  set(ITK_PATCH_FILES "${ITK_SRC_DIR}/configure" "${ITK_SRC_DIR}/tclconfig/tcl.m4")
  ExternalProject_Add(ITK_BLD
    URL ${CMAKE_SOURCE_DIR}/external/itk4.1.0.tar.gz
    URL_HASH MD5=600df0a12868507c52d46fd59d4e1c16
    PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${ITK_PATCH_FILES}
    CONFIGURE_COMMAND ${ITK_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/lib --with-tk=${CMAKE_BINARY_DIR}/lib --with-itcl=${CMAKE_BINARY_DIR}/lib/itcl4.1.1
    BUILD_COMMAND make -j9
    INSTALL_COMMAND make install
    DEPENDS TCL_BLD TK_BLD ITCL_BLD
    )
else(NOT MSVC)
  ExternalProject_Add(ITK_BLD
    URL ${CMAKE_SOURCE_DIR}/external/itk4.1.0.tar.gz
    URL_HASH MD5=600df0a12868507c52d46fd59d4e1c16
    CONFIGURE_COMMAND ""
    BINARY_DIR ${ITK_SRC_DIR}/win
    BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
    INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
    DEPENDS TCL_BLD TK_BLD ITCL_BLD
    )
endif(NOT MSVC)
ExternalProject_Target(itk ITK_BLD
  IMPORT_PREFIX itk4.1.0
  OUTPUT_FILE libitk4.1.0${CMAKE_SHARED_LIBRARY_SUFFIX}
  )

ExternalProject_ByProducts(ITK_BLD ${INCLUDE_DIR}
  itk.h
  itkInt.h
  )

ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
  itk4.1.0/itk.tcl
  itk4.1.0/pkgIndex.tcl
  itk4.1.0/Widget.itk
  itk4.1.0/Archetype.itk
  itk4.1.0/Toplevel.itk
  itk4.1.0/tclIndex
  )

###############################################################################
#                                 IWidgets
###############################################################################
set(IWIDGETS_SRC_DIR "${CMAKE_BINARY_DIR}/IWIDGETS_BLD-prefix/src/IWIDGETS_BLD")
if(NOT MSVC)
  set(IWIDGETS_PATCH_FILES "${IWIDGETS_SRC_DIR}/configure" "${IWIDGETS_SRC_DIR}/tclconfig/tcl.m4")
  ExternalProject_Add(IWIDGETS_BLD
    URL ${CMAKE_SOURCE_DIR}/external/iwidgets-4.1.1.tar.gz
    URL_HASH MD5=cca62e022b0d561a2bba19bd56ecc667
    PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${IWIDGETS_PATCH_FILES}
    CONFIGURE_COMMAND ${IWIDGETS_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/lib --with-tk=${CMAKE_BINARY_DIR}/lib --with-itcl=${CMAKE_BINARY_DIR}/lib/itcl4.1.1 --with-itk=${ITK_SRC_DIR}
    BUILD_COMMAND make -j9
    INSTALL_COMMAND make install
    DEPENDS TCL_BLD TK_BLD ITCL_BLD ITK_BLD
    )
else(NOT MSVC)
  ExternalProject_Add(IWIDGETS_BLD
    URL ${CMAKE_SOURCE_DIR}/external/iwidgets-4.1.1.tar.gz
    URL_HASH MD5=cca62e022b0d561a2bba19bd56ecc667
    CONFIGURE_COMMAND ""
    BINARY_DIR ${IWIDGETS_SRC_DIR}/win
    BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR}
    INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR}
    DEPENDS TCL_BLD TK_BLD ITCL_BLD
    )
endif(NOT MSVC)

include(${CMAKE_SOURCE_DIR}/external/iwidgets_lib_byproducts.cmake)
ExternalProject_ByProducts(IWIDGETS_BLD ${LIB_DIR} ${IWIDGETS_LIB_BYPRODUCTS})

###############################################################################
#                                 TkTable
###############################################################################
set(TKTABLE_SRC_DIR "${CMAKE_BINARY_DIR}/TKTABLE_BLD-prefix/src/TKTABLE_BLD")
set(TKTABLE_PATCH_FILES "${TKTABLE_SRC_DIR}/configure" "${TKTABLE_SRC_DIR}/tclconfig/tcl.m4")
ExternalProject_Add(TKTABLE_BLD
  URL ${CMAKE_SOURCE_DIR}/external/Tktable2.10.tar.gz
  URL_HASH MD5=ecdd96b39ad49b820bf084eb1c786f66
  PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TKTABLE_PATCH_FILES}
  CONFIGURE_COMMAND ${TKTABLE_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/lib --with-tk=${CMAKE_BINARY_DIR}/lib
  BUILD_COMMAND make -j9
  INSTALL_COMMAND make install
  DEPENDS TCL_BLD TK_BLD
  )
ExternalProject_Target(tktable TKTABLE_BLD
  IMPORT_PREFIX Tktable2.10
  OUTPUT_FILE libTktable2.10${CMAKE_SHARED_LIBRARY_SUFFIX}
  )

ExternalProject_ByProducts(TKTABLE_BLD ${LIB_DIR}
  Tktable2.10/README.txt
  Tktable2.10/html/tkTable.html
  Tktable2.10/pkgIndex.tcl
  Tktable2.10/tkTable.tcl
  Tktable2.10/tktable.py
  Tktable2.10/license.txt
  )

###############################################################################
#                                 Tkhtml
###############################################################################
cmake_set_rpath(SUFFIX "Tkhtml3.0.1")
ExternalProject_Add(TKHTML_BLD
  URL ${CMAKE_SOURCE_DIR}/external/tkhtml-3.0.1.tar.gz
  URL_HASH MD5=9abfe32b386558178f4d9cb8fb67a78c
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DTCL_STUB_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtclstub8.7${CMAKE_STATIC_LIBRARY_SUFFIX} -DTK_STUB_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtkstub8.7${CMAKE_STATIC_LIBRARY_SUFFIX} -DTK_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtk8.7${CMAKE_SHARED_LIBRARY_SUFFIX} -DTCL_TCLSH=${CMAKE_BINARY_DIR}/${BIN_DIR}/tclsh8.7${CMAKE_EXECUTABLE_SUFFIX} -DTCL_INCLUDE_DIRS=${CMAKE_BINARY_DIR}/include
  DEPENDS TCL_BLD TK_BLD
  )
ExternalProject_Target(tkhtml TKHTML_BLD
  IMPORT_PREFIX Tkhtml3.0.1
  OUTPUT_FILE libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}.3.0
  SYMLINKS "libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}"
  LINK_TARGET "libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}"
  RPATH
  )
cmake_set_rpath("")

ExternalProject_ByProducts(TKHTML_BLD ${LIB_DIR}
  Tkhtml3.0.1/pkgIndex.tcl
  )

###############################################################################
#                                 GDiam
###############################################################################
ExternalProject_Add(GDIAM_BLD
  URL ${CMAKE_SOURCE_DIR}/external/libgdiam-1.0.3.tar.gz
  URL_HASH MD5=f52c1a0e565a9cedadeeb3b0bedcca6f
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(gdiam GDIAM_BLD
  OUTPUT_FILE libgdiam${CMAKE_SHARED_LIBRARY_SUFFIX}
  RPATH
  )

# TODO - update gdiam build to install header

###############################################################################
#                                 NETPBM
###############################################################################
ExternalProject_Add(NETPBM_BLD
  URL ${CMAKE_SOURCE_DIR}/external/libnetpbm-minimal.tar.gz
  URL_HASH MD5=4e89f5b70c7b3f24e08b9a3b5e62c2b4
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(netpbm NETPBM_BLD
  OUTPUT_FILE libnetpbm${CMAKE_SHARED_LIBRARY_SUFFIX}
  RPATH
  )

# TODO - update netpbm build to install headers

###############################################################################
#                                 libregex
###############################################################################
ExternalProject_Add(REGEX_BLD
  URL ${CMAKE_SOURCE_DIR}/external/libregex.tar.gz
  URL_HASH MD5=ef1f76c1af29bb0e4105b7c0eb5a215e
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(regex REGEX_BLD
  OUTPUT_FILE libregex${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0.4
  SYMLINKS "libregex${CMAKE_SHARED_LIBRARY_SUFFIX};libregex${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
  LINK_TARGET "libregex${CMAKE_SHARED_LIBRARY_SUFFIX}"
  RPATH
  )

# TODO - update libregex build to install headers

###############################################################################
#                 Screened Poisson Surface Reconstruction
###############################################################################
ExternalProject_Add(SPSR_BLD
  URL ${CMAKE_SOURCE_DIR}/external/libspsr.tar.gz
  URL_HASH MD5=e5b10c7982cd2b9b287579d43588fbfa
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=ON -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(spsr SPSR_BLD
  OUTPUT_FILE libSPSR${CMAKE_SHARED_LIBRARY_SUFFIX}
  RPATH
  )

# TODO - update SPSR build to install headers

###############################################################################
#                                 libtermlib
###############################################################################
if(NOT MSVC)
  ExternalProject_Add(TERMLIB_BLD
    URL ${CMAKE_SOURCE_DIR}/external/libtermlib.tar.gz
    URL_HASH MD5=00b6ff29ddba4a75ede48618cb7cacc5
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    )
  ExternalProject_Target(termlib TERMLIB_BLD
    OUTPUT_FILE libtermlib${CMAKE_SHARED_LIBRARY_SUFFIX}.1.7.1
    SYMLINKS "libtermlib${CMAKE_SHARED_LIBRARY_SUFFIX};libtermlib${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
    LINK_TARGET "libtermlib${CMAKE_SHARED_LIBRARY_SUFFIX}"
    RPATH
    )
endif(NOT MSVC)

# TODO - update termlib build to install headers

###############################################################################
#                                 libutahrle
###############################################################################
ExternalProject_Add(UTAHRLE_BLD
  URL ${CMAKE_SOURCE_DIR}/external/libutahrle.tar.gz
  URL_HASH MD5=a50af0cbb6cc7e47b44142be82968383
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(utahrle UTAHRLE_BLD
  OUTPUT_FILE libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}.19.0.1
  STATIC_OUTPUT_FILE libutahrle${CMAKE_STATIC_LIBRARY_SUFFIX}
  SYMLINKS "libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX};libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}.19"
  LINK_TARGET "libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}"
  RPATH
  )
ExternalProject_ByProducts(UTAHRLE_BLD ${INCLUDE_DIR}
  rle.h
  rle_code.h
  rle_config.h
  rle_put.h
  rle_raw.h
  )

###############################################################################
#                                 libvds
###############################################################################
ExternalProject_Add(VDS_BLD
  URL ${CMAKE_SOURCE_DIR}/external/libvds.tar.gz
  URL_HASH MD5=e5b216a89af6b02a65d50abae8e52cb2
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(vds VDS_BLD
  OUTPUT_FILE libvds${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0.1
  SYMLINKS "libvds${CMAKE_SHARED_LIBRARY_SUFFIX};libvds${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
  LINK_TARGET "libvds${CMAKE_SHARED_LIBRARY_SUFFIX}"
  RPATH
  )
ExternalProject_ByProducts(VDS_BLD ${INCLUDE_DIR}
  vds.h
  )

###############################################################################
#                                 LZ4
###############################################################################
ExternalProject_Add(LZ4_BLD
  URL ${CMAKE_SOURCE_DIR}/external/lz4-1.8.3-minimal-cmakeified.tar.gz
  URL_HASH MD5=cc9610d7466926011380cd617ed62a49
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(lz4 LZ4_BLD
  OUTPUT_FILE liblz4${CMAKE_SHARED_LIBRARY_SUFFIX}
  RPATH
  )
ExternalProject_ByProducts(LZ4_BLD ${INCLUDE_DIR}
  lz4.h
  lz4hc.h
  lz4frame.h
  )

###############################################################################
#                                 OpenNURBS
###############################################################################
ExternalProject_Add(OPENNURBS_BLD
  URL ${CMAKE_SOURCE_DIR}/external/openNURBS.tar.gz
  URL_HASH MD5=9e839833037a98a26d41d79f56e290a5
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(openNURBS OPENNURBS_BLD
  OUTPUT_FILE libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}.2012.10.245
  SYMLINKS "libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}"
  LINK_TARGET "libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}"
  RPATH
  )
ExternalProject_ByProducts(OPENNURBS_BLD ${INCLUDE_DIR}
  openNURBS/opennurbs.h
  openNURBS/opennurbs_3dm.h
  openNURBS/opennurbs_3dm_attributes.h
  openNURBS/opennurbs_3dm_properties.h
  openNURBS/opennurbs_3dm_settings.h
  openNURBS/opennurbs_annotation.h
  openNURBS/opennurbs_annotation2.h
  openNURBS/opennurbs_arc.h
  openNURBS/opennurbs_arccurve.h
  openNURBS/opennurbs_archive.h
  openNURBS/opennurbs_array.h
  openNURBS/opennurbs_array_defs.h
  openNURBS/opennurbs_base32.h
  openNURBS/opennurbs_base64.h
  openNURBS/opennurbs_beam.h
  openNURBS/opennurbs_bezier.h
  openNURBS/opennurbs_bitmap.h
  openNURBS/opennurbs_bounding_box.h
  openNURBS/opennurbs_box.h
  openNURBS/opennurbs_brep.h
  openNURBS/opennurbs_circle.h
  openNURBS/opennurbs_color.h
  openNURBS/opennurbs_compress.h
  openNURBS/opennurbs_cone.h
  openNURBS/opennurbs_crc.h
  openNURBS/opennurbs_curve.h
  openNURBS/opennurbs_curveonsurface.h
  openNURBS/opennurbs_curveproxy.h
  openNURBS/opennurbs_cylinder.h
  openNURBS/opennurbs_defines.h
  openNURBS/opennurbs_detail.h
  openNURBS/opennurbs_dimstyle.h
  openNURBS/opennurbs_dll_resource.h
  openNURBS/opennurbs_ellipse.h
  openNURBS/opennurbs_error.h
  openNURBS/opennurbs_evaluate_nurbs.h
  openNURBS/opennurbs_extensions.h
  openNURBS/opennurbs_font.h
  openNURBS/opennurbs_fpoint.h
  openNURBS/opennurbs_fsp.h
  openNURBS/opennurbs_fsp_defs.h
  openNURBS/opennurbs_geometry.h
  openNURBS/opennurbs_group.h
  openNURBS/opennurbs_hatch.h
  openNURBS/opennurbs_hsort_template.h
  openNURBS/opennurbs_instance.h
  openNURBS/opennurbs_intersect.h
  openNURBS/opennurbs_knot.h
  openNURBS/opennurbs_layer.h
  openNURBS/opennurbs_light.h
  openNURBS/opennurbs_line.h
  openNURBS/opennurbs_linecurve.h
  openNURBS/opennurbs_linestyle.h
  openNURBS/opennurbs_linetype.h
  openNURBS/opennurbs_lookup.h
  openNURBS/opennurbs_mapchan.h
  openNURBS/opennurbs_massprop.h
  openNURBS/opennurbs_material.h
  openNURBS/opennurbs_math.h
  openNURBS/opennurbs_matrix.h
  openNURBS/opennurbs_memory.h
  openNURBS/opennurbs_mesh.h
  openNURBS/opennurbs_nurbscurve.h
  openNURBS/opennurbs_nurbssurface.h
  openNURBS/opennurbs_object.h
  openNURBS/opennurbs_object_history.h
  openNURBS/opennurbs_objref.h
  openNURBS/opennurbs_offsetsurface.h
  openNURBS/opennurbs_optimize.h
  openNURBS/opennurbs_plane.h
  openNURBS/opennurbs_planesurface.h
  openNURBS/opennurbs_pluginlist.h
  openNURBS/opennurbs_point.h
  openNURBS/opennurbs_pointcloud.h
  openNURBS/opennurbs_pointgeometry.h
  openNURBS/opennurbs_pointgrid.h
  openNURBS/opennurbs_polycurve.h
  openNURBS/opennurbs_polyedgecurve.h
  openNURBS/opennurbs_polyline.h
  openNURBS/opennurbs_polylinecurve.h
  openNURBS/opennurbs_qsort_template.h
  openNURBS/opennurbs_rand.h
  openNURBS/opennurbs_rendering.h
  openNURBS/opennurbs_revsurface.h
  openNURBS/opennurbs_rtree.h
  openNURBS/opennurbs_sphere.h
  openNURBS/opennurbs_string.h
  openNURBS/opennurbs_sumsurface.h
  openNURBS/opennurbs_surface.h
  openNURBS/opennurbs_surfaceproxy.h
  openNURBS/opennurbs_system.h
  openNURBS/opennurbs_textlog.h
  openNURBS/opennurbs_texture.h
  openNURBS/opennurbs_texture_mapping.h
  openNURBS/opennurbs_torus.h
  openNURBS/opennurbs_unicode.h
  openNURBS/opennurbs_userdata.h
  openNURBS/opennurbs_uuid.h
  openNURBS/opennurbs_version.h
  openNURBS/opennurbs_viewport.h
  openNURBS/opennurbs_workspace.h
  openNURBS/opennurbs_x.h
  openNURBS/opennurbs_xform.h
  openNURBS/opennurbs_zlib.h
  )

###############################################################################
#                                 Poly2Tri
###############################################################################
ExternalProject_Add(POLY2TRI_BLD
  URL ${CMAKE_SOURCE_DIR}/external/poly2tri.tar.gz
  URL_HASH MD5=e2b2ec40f7907c46cc08199ab3b3f6f1
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=ON -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(poly2tri POLY2TRI_BLD
  OUTPUT_FILE libp2t${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0.1
  SYMLINKS "libp2t${CMAKE_SHARED_LIBRARY_SUFFIX};libp2t${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
  LINK_TARGET "libp2t${CMAKE_SHARED_LIBRARY_SUFFIX}"
  RPATH
  )
ExternalProject_ByProducts(POLY2TRI_BLD ${INCLUDE_DIR}
  poly2tri/poly2tri.h
  poly2tri/common/shapes.h
  poly2tri/sweep/cdt.h
  )

###############################################################################
#                                 RPly
###############################################################################
ExternalProject_Add(RPLY_BLD
  URL ${CMAKE_SOURCE_DIR}/external/rply.tar.gz
  URL_HASH MD5=4f30f8db7e36374415559405f348e58c
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=ON -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
  )
ExternalProject_Target(rply RPLY_BLD
  OUTPUT_FILE librply${CMAKE_SHARED_LIBRARY_SUFFIX}
  RPATH
  )

#TODO - update rply build to install header

###############################################################################
#                                PROJ-4
###############################################################################
#ExternalProject_Add(PROJ4_BLD
#  URL ${CMAKE_SOURCE_DIR}/external/proj-5.2.0.tar.gz
#  URL_HASH MD5=c85b0da471fcd80655e3448ebf4b9027
#  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DPROJ_TESTS=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_LIBDIR=${LIB_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
#  )
#ExternalProject_Target(proj4 PROJ4_BLD
#  OUTPUT_FILE libproj${CMAKE_SHARED_LIBRARY_SUFFIX}.14.0.2
#  SYMLINKS "libproj${CMAKE_SHARED_LIBRARY_SUFFIX};libproj${CMAKE_SHARED_LIBRARY_SUFFIX}.14"
#  LINK_TARGET "libproj${CMAKE_SHARED_LIBRARY_SUFFIX}"
#  RPATH
#  )

###############################################################################
#                                 GDAL
###############################################################################

#ExternalProject_Add(JPEG_BLD
#  URL ${CMAKE_SOURCE_DIR}/external/libjpeg-cmake.tar.gz
#  URL_HASH MD5=9856064c3a3d572b6248bbba97cea820
#  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_LIBDIR=${LIB_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
#  )
#ExternalProject_Target(jpeg JPEG_BLD
#  OUTPUT_FILE libjpeg${CMAKE_SHARED_LIBRARY_SUFFIX}
#  RPATH
#  )
#
#set(XML2_SRC_DIR "${CMAKE_BINARY_DIR}/XML2_BLD-prefix/src/XML2_BLD")
#ExternalProject_Add(XML2_BLD
#  URL ${CMAKE_SOURCE_DIR}/external/libxml2-2.9.8.tar.gz
#  URL_HASH MD5=b786e353e2aa1b872d70d5d1ca0c740d
#  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-Wl,-rpath,${CMAKE_BUILD_RPATH} ${XML2_SRC_DIR}/configure --without-python --with-zlib=${CMAKE_BINARY_DIR} --prefix=${CMAKE_BINARY_DIR}
#  BUILD_COMMAND make -j9
#  INSTALL_COMMAND make install
#  DEPENDS ${ZLIB_LIBRARY}
#  )
#ExternalProject_Target(xml2 XML2_BLD
#  OUTPUT_FILE libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}.2.9.8
#  SYMLINKS "libxml2${CMAKE_SHARED_LIBRARY_SUFFIX};libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}.2"
#  LINK_TARGET "libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}"
#  RPATH
#  )
#
#set(GDAL_SRC_DIR "${CMAKE_BINARY_DIR}/GDAL_BLD-prefix/src/GDAL_BLD")
#if(NOT MSVC)
#ExternalProject_Add(GDAL_BLD
#  URL ${CMAKE_SOURCE_DIR}/external/gdal-2.3.2.tar.gz
#  URL_HASH MD5=221e4bfe3e8e9443fd33f8fe46f8bf60
#  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} CXXFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-Wl,-rpath,${CMAKE_BUILD_RPATH} ${GDAL_SRC_DIR}/configure --without-libtool --with-jpeg=${CMAKE_BINARY_DIR} --with-libz=${CMAKE_BINARY_DIR} --with-png=${CMAKE_BINARY_DIR} --with-xml2=${CMAKE_BINARY_DIR} --with-proj=${CMAKE_BINARY_DIR} --prefix=${CMAKE_BINARY_DIR}
#  BUILD_COMMAND make -j9
#  BUILD_IN_SOURCE 1
#  INSTALL_COMMAND make install
#  DEPENDS PROJ4_BLD ${PNG_LIBRARY} ${ZLIB_LIBRARY} JPEG_BLD XML2_BLD
#  )
#else(NOT MSVC)
#ExternalProject_Add(GDAL_BLD
#  URL ${CMAKE_SOURCE_DIR}/external/gdal-2.3.2.tar.gz
#  URL_HASH MD5=221e4bfe3e8e9443fd33f8fe46f8bf60
#  CONFIGURE_COMMAND "" 
#  BUILD_IN_SOURCE 1
#  BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc GDAL_HOME=${CMAKE_BINARY_DIR} WIN64=1
#  INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install GDAL_HOME=${CMAKE_BINARY_DIR} WIN64=1
#  DEPENDS PROJ4_BLD
#  )
#endif(NOT MSVC)
#ExternalProject_Target(gdal GDAL_BLD
#  OUTPUT_FILE libgdal${CMAKE_SHARED_LIBRARY_SUFFIX}.2.3.2
#  SYMLINKS "libgdal${CMAKE_SHARED_LIBRARY_SUFFIX};libgdal${CMAKE_SHARED_LIBRARY_SUFFIX}.2"
#  LINK_TARGET "libgdal${CMAKE_SHARED_LIBRARY_SUFFIX}"
#  RPATH
#  )

###############################################################################
#                                 OpenSceneGraph
###############################################################################

# TODO - plugins are handled in a subdirectory, which means one RPATH setting
# will not work here unless we patch the build to put all the plugins in the
# same lib directory as the main libraries or actually accept and repeat our
# own RPATH setup.  (Don't see a viable way to do either without
# actually patching the sources to respect what we're trying to do -
# that's a possiblity, but needs a portable patch command:
# https://gitlab.kitware.com/cmake/cmake/issues/16854)
#
# Need to see if I can get https://github.com/starseeker/unix-tools/tree/master/patch to
# build and work on Windows
#
#ExternalProject_Add(OSG_BLD
#  URL ${CMAKE_SOURCE_DIR}/external/OpenSceneGraph-3.6.3.tar.gz
#  URL_HASH MD5=bd4cd3f50d0f030facbd71524540d5e7
#  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=ON -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DLIB_POSTFIX="" -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_OSG_DEPRECATED_SERIALIZERS=OFF -DBUILD_OSG_APPLICATIONS=OFF -DCMAKE_DEBUG_POSTFIX="" -DOSG_USE_DEPRECATED_API=OFF -DBUILD_OSG_EXAMPLES=OFF
#  )
#ExternalProject_Target(osg OSG_BLD
#  OUTPUT_FILE libosg${CMAKE_SHARED_LIBRARY_SUFFIX}.14.0.2
#  SYMLINKS "libosg${CMAKE_SHARED_LIBRARY_SUFFIX};libosg${CMAKE_SHARED_LIBRARY_SUFFIX}.14"
#  LINK_TARGET "libosg${CMAKE_SHARED_LIBRARY_SUFFIX}"
#  RPATH
#  )

###############################################################################
#                                  STEPcode
###############################################################################

ExternalProject_Add(LEMON_BLD
  URL ${CMAKE_SOURCE_DIR}/external/lemon.tar.gz
  URL_HASH MD5=69a6efbe1e577a872b9ae036da3af793
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
  )
ExternalProject_Target(lemon LEMON_BLD EXEC
  OUTPUT_FILE lemon${CMAKE_EXECUTABLE_SUFFIX}
  )

ExternalProject_Add(RE2C_BLD
  URL ${CMAKE_SOURCE_DIR}/external/re2c.tar.gz
  URL_HASH MD5=ac1f6b63f478fc3435a85f7aca2864ea
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLEMON_TEMPLATE=${CMAKE_BINARY_DIR}/share/lemon/lempar.c
  DEPENDS lemon
  )
ExternalProject_Target(re2c RE2C_BLD EXEC
  OUTPUT_FILE re2c${CMAKE_EXECUTABLE_SUFFIX}
  )

ExternalProject_Add(PERPLEX_BLD
  URL ${CMAKE_SOURCE_DIR}/external/perplex.tar.gz
  URL_HASH MD5=9e68cadea7764c56ff73dfdfea985551
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLEMON_TEMPLATE=${CMAKE_BINARY_DIR}/share/lemon/lempar.c
  DEPENDS lemon re2c
  )
ExternalProject_Target(perplex PERPLEX_BLD EXEC
  OUTPUT_FILE perplex${CMAKE_EXECUTABLE_SUFFIX}
  )

ExternalProject_Add(STEPCODE_BLD
  URL ${CMAKE_SOURCE_DIR}/external/STEPcode-0.7.tar.gz
  URL_HASH MD5=93066e11024ceb0f3225f7df881eff99
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DSC_BUILD_SCHEMAS= -DLEMON_TEMPLATE=${CMAKE_BINARY_DIR}/share/lemon/lempar.c -DHAVE_PARENT_BUILD=1 -DSC_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
  DEPENDS lemon re2c perplex
  )
set(STEPCODE_LIBS base express exppp stepcore stepeditor stepdai steputils)
foreach(SCLIB ${STEPCODE_LIBS})
  ExternalProject_Target(lib${SCLIB} STEPCODE_BLD
    OUTPUT_FILE lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.2.0.0
    SYMLINKS "lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX};lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.2"
    LINK_TARGET "lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    RPATH
    )
endforeach(SCLIB ${STEPCODE_LIBS})
set(STEPCODE_EXECS check-express exppp exp2cxx exp2python)
foreach(SCEXEC ${STEPCODE_EXECS})
  ExternalProject_Target(${SCEXEC} STEPCODE_BLD
    OUTPUT_FILE ${SCEXEC}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH EXEC
    )
endforeach(SCEXEC ${STEPCODE_EXECS})
ExternalProject_ByProducts(STEPCODE_BLD ${INCLUDE_DIR}
  stepcode/cldai/sdaiApplication_instance_set.h
  stepcode/cldai/sdaiSession_instance.h
  stepcode/cldai/sdaiObject.h
  stepcode/cldai/sdaiString.h
  stepcode/cldai/sdaiEntity_extent.h
  stepcode/cldai/sdaiEnum.h
  stepcode/cldai/sdaiModel_contents.h
  stepcode/cldai/sdaiBinary.h
  stepcode/cldai/sdaiEntity_extent_set.h
  stepcode/cldai/sdaiModel_contents_list.h
  stepcode/cldai/sdaiDaObject.h
  stepcode/ordered_attrs.h
  stepcode/exppp/exppp.h
  stepcode/express/hash.h
  stepcode/express/error.h
  stepcode/express/linklist.h
  stepcode/express/basic.h
  stepcode/express/memory.h
  stepcode/express/lexact.h
  stepcode/express/type.h
  stepcode/express/caseitem.h
  stepcode/express/entity.h
  stepcode/express/resolve.h
  stepcode/express/schema.h
  stepcode/express/stmt.h
  stepcode/express/expr.h
  stepcode/express/dict.h
  stepcode/express/expbasic.h
  stepcode/express/alg.h
  stepcode/express/variable.h
  stepcode/express/express.h
  stepcode/express/object.h
  stepcode/express/symbol.h
  stepcode/express/scope.h
  stepcode/sc_export.h
  stepcode/sc_cf.h
  stepcode/clutils/Str.h
  stepcode/clutils/gennodearray.h
  stepcode/clutils/gennode.h
  stepcode/clutils/errordesc.h
  stepcode/clutils/gennodelist.h
  stepcode/clutils/sc_hash.h
  stepcode/clutils/dirobj.h
  stepcode/cleditor/cmdmgr.h
  stepcode/cleditor/editordefines.h
  stepcode/cleditor/SdaiHeaderSchemaClasses.h
  stepcode/cleditor/seeinfodefault.h
  stepcode/cleditor/SdaiHeaderSchema.h
  stepcode/cleditor/SdaiSchemaInit.h
  stepcode/cleditor/STEPfile.h
  stepcode/sc_version_string.h
  stepcode/sc_stdbool.h
  stepcode/base/sc_getopt.h
  stepcode/base/sc_trace_fprintf.h
  stepcode/base/sc_benchmark.h
  stepcode/base/sc_memmgr.h
  stepcode/clstepcore/STEPundefined.h
  stepcode/clstepcore/mgrnodelist.h
  stepcode/clstepcore/STEPattribute.h
  stepcode/clstepcore/STEPaggregate.h
  stepcode/clstepcore/ExpDict.h
  stepcode/clstepcore/read_func.h
  stepcode/clstepcore/needFunc.h
  stepcode/clstepcore/mgrnodearray.h
  stepcode/clstepcore/mgrnode.h
  stepcode/clstepcore/dispnode.h
  stepcode/clstepcore/sdai.h
  stepcode/clstepcore/STEPcomplex.h
  stepcode/clstepcore/instmgr.h
  stepcode/clstepcore/baseType.h
  stepcode/clstepcore/sdaiSelect.h
  stepcode/clstepcore/SubSuperIterators.h
  stepcode/clstepcore/dictdefs.h
  stepcode/clstepcore/SingleLinkList.h
  stepcode/clstepcore/STEPattributeList.h
  stepcode/clstepcore/dispnodelist.h
  stepcode/clstepcore/sdaiApplication_instance.h
  stepcode/clstepcore/Registry.h
  stepcode/clstepcore/complexSupport.h
  )

# Simple "parent" build that tests the use of the imported library defined by ExternalProject_Target
include_directories(${CMAKE_BINARY_DIR}/include)
add_executable(gzjoin gzjoin.c)
target_link_libraries(gzjoin ${ZLIB_LIBRARY})
install(TARGETS gzjoin
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# By default, we want debugging information
set(CPACK_STRIP_FILES FALSE)
set(CPACK_GENERATOR TGZ ZIP)
include(CPack)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8




