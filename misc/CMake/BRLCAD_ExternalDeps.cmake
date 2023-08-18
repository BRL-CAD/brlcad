#       B R L C A D _ E X T E R N A L D E P S . C M A K E
# BRL-CAD
#
# Copyright (c) 2023 United States Government as represented by
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
#
# Logic to set up third party dependences (either system installed
# versions or prepared local versions to be bundled with BRL-CAD.)

if (NOT EXISTS "${BRLCAD_EXT_INSTALL_DIR}")
  message(WARNING "BRLCAD_EXT_INSTALL_DIR is set to ${BRLCAD_EXT_INSTALL_DIR} but that location does not exist.  This will result in only system libraries being used for compilation, with no external dependencies being bundled into installers.")
endif (NOT EXISTS "${BRLCAD_EXT_INSTALL_DIR}")

if (NOT EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}")
  message(WARNING "BRLCAD_EXT_NOINSTALL_DIR is set to ${BRLCAD_EXT_NOINSTALL_DIR} but that location does not exist.  This means BRL-CAD's build will be dependent on system versions of build tools such as patchelf and astyle being present.")
endif (NOT EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}")


# The relative RPATH is platform specific
if (APPLE)
  set(RELATIVE_RPATH ";@loader_path/../${LIB_DIR}")
else (APPLE)
  set(RELATIVE_RPATH ":\\$ORIGIN/../${LIB_DIR}")
endif (APPLE)

# The first pass through the configure stage, we move the files we want to use
# into place and prepare them for use with the build.

# TODO - At the moment this is a one-shot affair... if the external directory
# contents change after the initial configure or the user erases third party
# elements, we won't catch it.  There are things we can do about that, the
# question is how much effort to put into it...
set(THIRDPARTY_INVENTORY "${CMAKE_BINARY_DIR}/CMakeFiles/thirdparty.txt")
if (NOT EXISTS "${THIRDPARTY_INVENTORY}")

  # We bulk copy the contents of the BRLCAD_EXT_INSTALL_DIR tree into our own
  # directory.  For some of the external dependencies (like Tcl) library
  # elements must be in sane relative locations to binaries being executed, and
  # leaving them in BRLCAD_EXT_INSTALL_DIR won't work.  On Windows, the dlls for all
  # the dependencies will need to be located correctly relative to the bin
  # build directory.
  #
  # Rather than complicate matters trying to pick and choose what to move, just
  # stage everything.  Depending on what the dependencies write into their
  # install directories we may have to be more selective about this in the
  # future, but for now let's try simplicity - the less we can couple this
  # logic to the specific contents of extinstall, the better.
  file(GLOB SDIRS LIST_DIRECTORIES true RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
  foreach(sd ${SDIRS})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${sd})
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/${sd}.tar "${BRLCAD_EXT_INSTALL_DIR}/${sd}" WORKING_DIRECTORY "${BRLCAD_EXT_INSTALL_DIR}")
    # Whether we're single or multi config, do a top level decompression to give
    # the install targets a uniform source for all configurations.
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    if (CMAKE_CONFIGURATION_TYPES)
      # For multi-config, we'll also need to decompress once for each active configuration's build dir
      # so the executables will work locally...
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${sd})
	execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
	message("decompress to ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif (CMAKE_CONFIGURATION_TYPES)
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${sd}.tar)
  endforeach(sd ${SDIRS})

  # NOTE - we may need to find and redo symlinks, if we get any that are full
  # paths - we expect .so and .so.* style symlinks on some platforms, and there
  # may be others.  If those paths are absolute in BRLCAD_EXT_INSTALL_DIR they will be
  # wrong when copied into the BRL-CAD build - they will work on the build
  # machine since full path links will resolve, but will fail when installed on
  # another machine.  A quick tests suggests we don't have any like that right
  # now, but it's not clear we can count on that...

  # Files copied - build the list of everything we got from extinstall
  file(GLOB_RECURSE THIRDPARTY_FILES LIST_DIRECTORIES false RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
  string(REPLACE ";" "\n" THIRDPARTY_W "${THIRDPARTY_FILES}")
  file(WRITE "${THIRDPARTY_INVENTORY}" "${THIRDPARTY_W}")

else (NOT EXISTS "${THIRDPARTY_INVENTORY}")

  file(STRINGS "${THIRDPARTY_INVENTORY}" THIRDPARTY_FILES)

endif (NOT EXISTS "${THIRDPARTY_INVENTORY}")

# See if we have patchelf available
find_program(PATCHELF_EXECUTABLE NAMES patchelf HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})

# Third party binaries can't relied on for rpath settings suitable for our
# use cases.
set(BINARY_FILES)

message("Identifying 3rd party lib and exe files...")

# If patchelf returns a non-zero error code, the file isn't a binary file.
#
# TODO - while this is one way to sort out what files are the exe/lib
# binary files, is it the only way?  Not sure if depending on patchelf's
# error exit behavior is really a good/viable approach to this long term...
set(target_dirs bin lib)
foreach(lf ${THIRDPARTY_FILES})
  set(CFILE)
  foreach(tdir ${target_dirs})
    if ("${lf}" MATCHES "${tdir}")
      set(CFILE 1)
    endif ("${lf}" MATCHES "${tdir}")
  endforeach(tdir ${target_dirs})
  if (NOT CFILE)
    continue()
  endif (NOT CFILE)
  if (PATCHELF_EXECUTABLE)
    execute_process(COMMAND ${PATCHELF_EXECUTABLE} ${lf} RESULT_VARIABLE NOT_BIN_OBJ OUTPUT_VARIABLE NB_OUT ERROR_VARIABLE NB_ERR)
    if (NOT_BIN_OBJ)
      continue()
    endif (NOT_BIN_OBJ)
  elseif (MSVC)
    # We don't do RPATH management on Windows
    continue()
  elseif (APPLE)
    execute_process(COMMAND otool -L ${lf} RESULT_VARIABLE ORESULT OUTPUT_VARIABLE OTOOL_OUT ERROR_VARIABLE NB_ERR)
    if ("${OTOOL_OUT}" MATCHES "Archive")
      message("${lf} is an Archive")
      continue()
    endif ("${OTOOL_OUT}" MATCHES "Archive")
    if ("${OTOOL_OUT}" MATCHES "not an object")
      message("${lf} is not an object")
      continue()
    endif ("${OTOOL_OUT}" MATCHES "not an object")
  endif(PATCHELF_EXECUTABLE)
  set(BINARY_FILES ${BINARY_FILES} ${lf})
endforeach(lf ${THIRDPARTY_FILES})
message("Identifying 3rd party lib and exe files... done.")

message("Setting rpath on 3rd party lib and exe files...")
if (NOT CMAKE_CONFIGURATION_TYPES)
  # Set local RPATH so the files will work during build
  foreach(lf ${BINARY_FILES})
    if (PATCHELF_EXECUTABLE)
      execute_process(COMMAND ${PATCHELF_EXECUTABLE} --remove-rpath ${lf})
      execute_process(COMMAND ${PATCHELF_EXECUTABLE} --set-rpath "${CMAKE_BINARY_DIR}/${LIB_DIR}" ${lf})
    elseif (APPLE)
      execute_process(COMMAND install_name_tool -delete_rpath "${BRLCAD_EXT_DIR}/extinstall/${LIB_DIR}" ${lf} RESULT_VARIABLE ORESULT)
      execute_process(COMMAND install_name_tool -add_rpath "${CMAKE_BINARY_DIR}/${LIB_DIR}" ${lf})
    endif (PATCHELF_EXECUTABLE)
  endforeach(lf ${BINARY_FILES})
else (NOT CMAKE_CONFIGURATION_TYPES)
  # For multi-config, we set the RPATHs for each active configuration's build dir
  # so the executables will work locally.  We don't need to set the top level copy
  # being used for the install target since in multi-config those copies won't be
  # used by build directory executables
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    foreach(lf ${BINARY_FILES})
      if (PATCHELF_EXECUTABLE)
	execute_process(COMMAND ${PATCHELF_EXECUTABLE} --remove-rpath ${lf} WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
	execute_process(COMMAND ${PATCHELF_EXECUTABLE} --set-rpath "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${LIB_DIR}" ${lf} WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
      elseif (APPLE)
	execute_process(COMMAND install_name_tool -delete_rpath "${BRLCAD_EXT_DIR}/extinstall/${LIB_DIR}" ${lf} WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}" RESULT_VARIABLE ORESULT)
	execute_process(COMMAND install_name_tool -add_rpath "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${LIB_DIR}" ${lf} WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
      endif (PATCHELF_EXECUTABLE)
    endforeach(lf ${BINARY_FILES})
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
endif (NOT CMAKE_CONFIGURATION_TYPES)

message("Setting rpath on 3rd party lib and exe files... done.")

foreach(tf ${THIRDPARTY_FILES})
  # Rather than doing the PROGRAMS install for all binary files, we target just
  # those in the bin directory - those are the ones we would expect to want
  # CMake's *_EXECUTE permissions during install.
  get_filename_component(dir "${tf}" DIRECTORY)
  if (NOT dir)
    message("Error - unexpected toplevel ext file: ${tf} ")
    continue()
  endif (NOT dir)
  if (${dir} MATCHES "${BIN_DIR}$")
    install(PROGRAMS "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
  else (${dir} MATCHES "${BIN_DIR}%")
    install(FILES "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
  endif (${dir} MATCHES "${BIN_DIR}$")
endforeach(tf ${THIRDPARTY_FILES})

# Need to fix RPATH on binary files.  Don't do it for symlinks since
# following them will just result in re-processing the same file's RPATH
# multiple times.
foreach(bf ${BINARY_FILES})
  if (IS_SYMLINK ${bf})
    continue()
  endif (IS_SYMLINK ${bf})
  if (PATCHELF_EXECUTABLE)
    install(CODE "execute_process(COMMAND ${PATCHELF_EXECUTABLE} --remove-rpath \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
    install(CODE "execute_process(COMMAND ${PATCHELF_EXECUTABLE} --set-rpath \"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${RELATIVE_RPATH}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
  elseif (APPLE)
    install(CODE "execute_process(COMMAND install_name_tool -delete_rpath \"${CMAKE_BINARY_DIR}/${LIB_DIR}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\" RESULT_VARIABLE ORESULT)")
    install(CODE "execute_process(COMMAND install_name_tool -add_rpath \"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${RELATIVE_RPATH}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
  endif (PATCHELF_EXECUTABLE)
endforeach(bf ${BINARY_FILES})

# zlib compression/decompression library
# https://zlib.net
#
# Note - our copy is modified from Vanilla upstream to support specifying a
# custom prefix - until a similar feature is available in upstream zlib, we
# need this to reliably avoid conflicts between bundled and system zlib.
set(ZLIB_ROOT "${CMAKE_BINARY_DIR}")
find_package(ZLIB REQUIRED)

# libregex regular expression matching
set(REGEX_ROOT "${CMAKE_BINARY_DIR}")
find_package(REGEX REQUIRED)

# netpbm library - support for pnm,ppm,pbm, etc. image files
# http://netpbm.sourceforge.net/
#
# Note - we build a custom subset of this library for convenience, and (at the
# moment) mod it to remove a GPL string component, although there is some hope
# (2022) that the latter issue will be addressed upstream.  Arguably in this
# form our netpbm copy isn't really a good fit for ext, but it is kept there
# because a) there is an active upstream and b) we are unlikely to need to
# modify these sources to our needs from a functional perspective.
set(NETPBM_ROOT "${CMAKE_BINARY_DIR}")
find_package(NETPBM)

# libpng - Portable Network Graphics image file support
# http://www.libpng.org/pub/png/libpng.html
set(PNG_ROOT "${CMAKE_BINARY_DIR}")
find_package(PNG)

# STEPcode - support for reading and writing STEP files
# https://github.com/stepcode/stepcode
#
# Note - We are heavily involved with the stepcode effort and in the past our
# stepcode copy has been extensively modified, but we are working to get our
# copy and a released upstream copy synced - in anticipation of that, stepcode
# lives in ext.
if (BRLCAD_ENABLE_STEP)
  set(STEPCODE_ROOT "${CMAKE_BINARY_DIR}")
  find_package(STEPCODE)
endif (BRLCAD_ENABLE_STEP)

# GDAL -  translator library for raster and vector geospatial data formats
# https://gdal.org
if (BRLCAD_ENABLE_GDAL)
  set(GDAL_ROOT "${CMAKE_BINARY_DIR}")
  find_package(GDAL)
endif (BRLCAD_ENABLE_GDAL)

# Open Asset Import Library - library for supporting I/O for a number of
# Geometry file formats
# https://github.com/assimp/assimp
if (BRLCAD_ENABLE_ASSETIMPORT)
  set(ASSETIMPORT_ROOT "${CMAKE_BINARY_DIR}")
  find_package(ASSETIMPORT)
endif (BRLCAD_ENABLE_ASSETIMPORT)

# OpenMesh Library - library for representing and manipulating polygonal meshes
# https://www.graphics.rwth-aachen.de/software/openmesh/
if (BRLCAD_ENABLE_OPENMESH)
  set(OpenMesh_ROOT "${CMAKE_BINARY_DIR}")
  find_package(OpenMesh)
endif (BRLCAD_ENABLE_OPENMESH)

# TCL - scripting language.  For Tcl/Tk builds we want
# static lib building on so we get the stub libraries.
if (BRLCAD_ENABLE_TK)
  # For FindTCL.cmake
  set(TCL_ENABLE_TK ON CACHE BOOL "enable tk")
endif (BRLCAD_ENABLE_TK)
mark_as_advanced(TCL_ENABLE_TK)
set(TCL_ROOT "${CMAKE_BINARY_DIR}")
find_package(TCL)
set(HAVE_TK 1)
set(ITK_VERSION "3.4")
set(IWIDGETS_VERSION "4.1.1")
CONFIG_H_APPEND(BRLCAD "#define ITK_VERSION \"${ITK_VERSION}\"\n")
CONFIG_H_APPEND(BRLCAD "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")

# A lot of code depends on knowing about Tk being active,
# so we set a flag in the configuration header to pass
# on that information.
CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_TK\n")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

