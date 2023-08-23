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


# Logic to set up third party dependences (either system installed
# versions or prepared local versions to be bundled with BRL-CAD.)


if (NOT EXISTS "${BRLCAD_EXT_INSTALL_DIR}")
  message(WARNING "BRLCAD_EXT_INSTALL_DIR is set to ${BRLCAD_EXT_INSTALL_DIR} but that location does not exist.  This will result in only system libraries being used for compilation, with no external dependencies being bundled into installers.")
endif (NOT EXISTS "${BRLCAD_EXT_INSTALL_DIR}")

if (NOT EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}")
  message(WARNING "BRLCAD_EXT_NOINSTALL_DIR is set to ${BRLCAD_EXT_NOINSTALL_DIR} but that location does not exist.  This means BRL-CAD's build will be dependent on system versions of build tools such as patchelf and astyle being present.")
endif (NOT EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}")

# If we got to extinstall through a symlink, we need to expand it so
# we can spot the path that would have been used in extinstall files
file(REAL_PATH "${BRLCAD_EXT_INSTALL_DIR}" BRLCAD_EXT_DIR_REAL EXPAND_TILDE)

# For repeat configure passes, we need to check any existing files copied
# against the extinstall dir's contents, to detect if the latter has changed
# and we need to redo the process.
set(THIRDPARTY_INVENTORY "${CMAKE_BINARY_DIR}/CMakeFiles/thirdparty.txt")
set(THIRDPARTY_INVENTORY_BINARIES "${CMAKE_BINARY_DIR}/CMakeFiles/thirdparty_binaries.txt")

# Find the tool we use to scrub EXT paths from files
find_program(STRCLEAR_EXECUTABLE strclear HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})

# The relative RPATH is specific to the location and platform
function(find_relative_rpath fp rp)
  # We don't want the filename to count, so offset our directory
  # count down by 1
  set(dcnt -1)
  set(fp_cpy ${fp})
  while (NOT "${fp_cpy}" STREQUAL "")
    get_filename_component(pdir "${fp_cpy}" DIRECTORY)
    set(fp_cpy ${pdir})
    math(EXPR dcnt "${dcnt} + 1")
  endwhile (NOT "${fp_cpy}" STREQUAL "")
  if (APPLE)
    set(RELATIVE_RPATH ";@loader_path")
  else (APPLE)
    set(RELATIVE_RPATH ":\\$ORIGIN")
  endif (APPLE)
  set(acnt 0)
  while(acnt LESS dcnt)
    set(RELATIVE_RPATH "${RELATIVE_RPATH}/..")
    math(EXPR acnt "${acnt} + 1")
  endwhile(acnt LESS dcnt)
  set(RELATIVE_RPATH "${RELATIVE_RPATH}/${LIB_DIR}")
  set(${rp} "${RELATIVE_RPATH}" PARENT_SCOPE)
endfunction(find_relative_rpath)

# See if we have patchelf available.  If it is available, we will be using it
# to manage the RPATH settings for third party exe/lib files.
find_program(PATCHELF_EXECUTABLE NAMES patchelf HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})


# These patterns are used to identify sets of files where we are assuming we
# don't need to do post-processing to correct file paths from the external
# install.
set(NOPROCESS_PATTERNS
  ".*/encodings/.*"
  ".*/include/.*"
  ".*/man/.*"
  ".*/msgs/.*"
  )

# These patterns are to be excluded from extinstall bundling - i.e., even if
# present in the specified extinstall directory, BRL-CAD will not incorporate
# them.  Generally speaking this is used to avoid files needed for external
# building but counterproductive in the BRL-CAD install.
set(EXCLUDED_PATTERNS
  ${LIB_DIR}/itcl4.2.3/itclConfig.sh
  ${LIB_DIR}/tclConfig.sh
  ${LIB_DIR}/tdbc1.1.5/tdbcConfig.sh
  ${LIB_DIR}/tkConfig.sh
  )

# We may need to scrub excluded files out of multiple copies, so wrap logic
# to do so into a function
function(STRIP_EXCLUDED RDIR EXPATTERNS)
  foreach (ep ${${EXPATTERNS}})
    file(GLOB_RECURSE MATCHING_FILES LIST_DIRECTORIES false RELATIVE "${RDIR}" "${RDIR}/${ep}")
    foreach(rf ${MATCHING_FILES})
      file(REMOVE "${RDIR}/${rf}")
      if (EXISTS ${RDIR}/${rf})
	message(FATAL_ERROR "Removing ${RDIR}/${rf} failed")
      endif (EXISTS ${RDIR}/${rf})
    endforeach(rf ${MATCHING_FILES})
  endforeach (ep ${${EXPATTERNS}})
endfunction(STRIP_EXCLUDED)

# For processing purposes, there are three categories of extinstall file:
#
# 1.  exe or shared library files needing RPATH adjustment (which also need
#     to be installed with executable permissions)
# 2.  binary files which are NOT executable files (images, etc.)
# 3.  text files
#
# If we don't want to hard-code information about specific files we expect
# to find in extinstall, we need a way to detect "on the fly" what we are
# dealing with.
function(FILE_TYPE fname BINARY_LIST TEXT_LIST NOEXEC_LIST)
  if (IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})
    return()
  endif (IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})
  foreach (skp ${NOPROCESS_PATTERNS})
    if ("${fname}" MATCHES "${skp}")
      set(${TEXT_LIST} ${${TEXT_LIST}} ${fname} PARENT_SCOPE)
      return()
    endif ("${fname}" MATCHES "${skp}")
  endforeach (skp ${NOPROCESS_PATTERNS})
  execute_process(COMMAND ${STRCLEAR_EXECUTABLE} -B ${CMAKE_BINARY_DIR}/${fname} RESULT_VARIABLE TXT_FILE)
  if ("${TXT_FILE}" GREATER 0)
    set(${TEXT_LIST} ${${TEXT_LIST}} ${fname} PARENT_SCOPE)
    return()
  endif ("${TXT_FILE}" GREATER 0)

  # Some kind of binary file, can we set an RPATH?
  if (PATCHELF_EXECUTABLE)
    execute_process(COMMAND ${PATCHELF_EXECUTABLE} ${CMAKE_BINARY_DIR}/${lf} RESULT_VARIABLE NOT_BIN_OBJ OUTPUT_VARIABLE NB_OUT ERROR_VARIABLE NB_ERR)
    if (NOT_BIN_OBJ)
      set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
      return()
    else (NOT_BIN_OBJ)
      set(${BINARY_LIST} ${${BINARY_LIST}} ${fname} PARENT_SCOPE)
      return()
    endif (NOT_BIN_OBJ)
  endif(PATCHELF_EXECUTABLE)
  if (APPLE)
    execute_process(COMMAND otool -l ${CMAKE_BINARY_DIR}/${lf} RESULT_VARIABLE ORESULT OUTPUT_VARIABLE OTOOL_OUT ERROR_VARIABLE NB_ERR)
    if ("${OTOOL_OUT}" MATCHES "Archive")
      set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
      return()
    endif ("${OTOOL_OUT}" MATCHES "Archive")
    if ("${OTOOL_OUT}" MATCHES "not an object")
      set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
      return()
    endif ("${OTOOL_OUT}" MATCHES "not an object")
    # Not one of the exceptions - binary
    set(${BINARY_LIST} ${${BINARY_LIST}} ${fname} PARENT_SCOPE)
    return()
  endif(APPLE)

  # If we haven't figured it out, treat as noexec binary
  set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
endfunction(FILE_TYPE)

# No matter what, we need to know what's in extinstall
file(GLOB_RECURSE THIRDPARTY_FILES LIST_DIRECTORIES false RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
# Filter out the files removed via STRIP_EXCLUDED
foreach(ep ${EXCLUDED_PATTERNS})
  list(FILTER THIRDPARTY_FILES EXCLUDE REGEX ${ep})
endforeach(ep ${EXCLUDED_PATTERNS})

# If we are repeating a configure process, we need to see what (if anything)
# has changed.  Rather than get sophisticated about this, we take a brute force
# approach - if the file list of extinstall differs or anything in extinstall
# is newer than the local copy, clear all old extinstall file copes and start
# fresh.
if (EXISTS "${THIRDPARTY_INVENTORY}")

  set(THIRDPARTY_CURRENT "${THIRDPARTY_FILES}")
  file(READ "${THIRDPARTY_INVENTORY}" THIRDPARTY_P)
  string(REPLACE "\n" ";" THIRDPARTY_PREVIOUS "${THIRDPARTY_P}")
  set(THIRDPARTY_PREV ${THIRDPARTY_PREVIOUS})

  list(REMOVE_ITEM THIRDPARTY_CURRENT ${THIRDPARTY_PREVIOUS})
  list(REMOVE_ITEM THIRDPARTY_PREV ${THIRDPARTY_FILES})

  # Unless we have a reason to reset, don't
  set(RESET_THIRDPARTY FALSE)

  # If the directory file lists differ, reset
  if (THIRDPARTY_CURRENT OR THIRDPARTY_PREV)
    set(RESET_THIRDPARTY TRUE)
  endif (THIRDPARTY_CURRENT OR THIRDPARTY_PREV)

  # If the lists of files are the same, check the timestamps.  If at least one
  # extinstall file is newer, need to reset.
  if (NOT RESET_THIRDPARTY)
    foreach (ef ${THIRDPARTY_FILES})
      if (${BRLCAD_EXT_INSTALL_DIR}/${ef} IS_NEWER_THAN ${CMAKE_BINARY_DIR}/${ef})
	message("${BRLCAD_EXT_INSTALL_DIR}/${ef} is newer than ${CMAKE_BINARY_DIR}/${ef}")
	set(RESET_THIRDPARTY TRUE)
	break()
      endif (${BRLCAD_EXT_INSTALL_DIR}/${ef} IS_NEWER_THAN ${CMAKE_BINARY_DIR}/${ef})
    endforeach (ef ${THIRDPARTY_FILES})
  endif (NOT RESET_THIRDPARTY)

  if (RESET_THIRDPARTY)
    message("Contents of ${BRLCAD_EXT_INSTALL_DIR} have changed.")
    message("Clearing previous bundled file copies...")
    foreach (ef ${THIRDPARTY_PREVIOUS})
      file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
    endforeach (ef ${THIRDPARTY_PREVIOUS})
    if (CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	file(REMOVE ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef})
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif (CMAKE_CONFIGURATION_TYPES)

    # Old contents cleared - remove log files so subsequent logic
    # knows to copy and process the new contents
    file(REMOVE ${THIRDPARTY_INVENTORY})
    file(REMOVE ${THIRDPARTY_INVENTORY_BINARIES})
    message("Clearing previous bundled file copies... done.")
  endif (RESET_THIRDPARTY)

endif (EXISTS "${THIRDPARTY_INVENTORY}")

# Ready for the main work of setting up the bundled library copies.
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
  if ("${BRLCAD_EXT_DIR}/extinstall" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
    set(EXT_DIR_STR "${BRLCAD_EXT_INSTALL_DIR}")
  else ("${BRLCAD_EXT_DIR}/extinstall" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
    set(EXT_DIR_STR "${BRLCAD_EXT_INSTALL_DIR} (via symlink ${BRLCAD_EXT_DIR}/extinstall)")
  endif ("${BRLCAD_EXT_DIR}/extinstall" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
  message("Staging third party files from ${EXT_DIR_STR} in build directory...")
  file(GLOB SDIRS LIST_DIRECTORIES true RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
  foreach(sd ${SDIRS})
    # Bundled up the sub-directory's contents so that the archive will expand with
    # paths relative to the extinstall root
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/${sd}.tar "${BRLCAD_EXT_INSTALL_DIR}/${sd}" WORKING_DIRECTORY "${BRLCAD_EXT_INSTALL_DIR}")

    # Make sure the build directory has the target directory to write to
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${sd})

    # Whether we're single or multi config, do a top level decompression to give
    # the install targets a uniform source for all configurations.
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

    # For multi-config, we'll also need to decompress once for each active configuration's build dir
    # so the executables will work locally...
    if (CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${sd})
	execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif (CMAKE_CONFIGURATION_TYPES)

    # Copying complete - remove the archive file
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${sd}.tar)
  endforeach(sd ${SDIRS})

  # The above copy is indiscriminate, so we follow behind it and strip out the
  # files we don't wish to include
  STRIP_EXCLUDED("${CMAKE_BINARY_DIR}" EXCLUDED_PATTERNS)
  if (CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      STRIP_EXCLUDED("${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}" EXCLUDED_PATTERNS)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif (CMAKE_CONFIGURATION_TYPES)

  # Unpacking the files doesn't update their timestamps.  If we want to be able to
  # check if extinstall has changed, we need to make sure the build dir copies are
  # newer than the extinstall copies for IS_NEWER_THAN testing.
  foreach(tf ${THIRDPARTY_FILES})
    execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/${tf}")
  endforeach(tf ${THIRDPARTY_FILES})
  message("Staging third party files from ${EXT_DIR_STR} in build directory... done.")

  # NOTE - we may need to find and redo symlinks, if we get any that are full
  # paths - we expect .so and .so.* style symlinks on some platforms, and there
  # may be others.  If those paths are absolute in BRLCAD_EXT_INSTALL_DIR they will be
  # wrong when copied into the BRL-CAD build - they will work on the build
  # machine since full path links will resolve, but will fail when installed on
  # another machine.  A quick tests suggests we don't have any like that right
  # now, but it's not clear we can count on that...

    # Write the current third party file list
  string(REPLACE ";" "\n" THIRDPARTY_W "${THIRDPARTY_FILES}")
  file(WRITE "${THIRDPARTY_INVENTORY}" "${THIRDPARTY_W}")

  message("Characterizing bundled third party files...")
  # Use various tools to sort out which files are exec/lib files.
  set(BINARY_FILES)
  set(TEXT_FILES)
  set(NONEXEC_FILES)
  foreach(lf ${THIRDPARTY_FILES})
    FILE_TYPE("${lf}" BINARY_FILES TEXT_FILES NOEXEC_FILES)
  endforeach(lf ${THIRDPARTY_FILES})
  message("Characterizing bundled third party files... done.")

  # Write the scrubbed binary list
  string(REPLACE ";" "\n" THIRDPARTY_B "${BINARY_FILES}")
  file(WRITE "${THIRDPARTY_INVENTORY_BINARIES}" "${THIRDPARTY_B}")

  message("Setting rpath on 3rd party lib and exe files...")
  if (NOT CMAKE_CONFIGURATION_TYPES)
    # Set local RPATH so the files will work during build
    foreach(lf ${BINARY_FILES})
      if (PATCHELF_EXECUTABLE)
	execute_process(COMMAND ${PATCHELF_EXECUTABLE} --remove-rpath ${lf} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
	execute_process(COMMAND ${PATCHELF_EXECUTABLE} --set-rpath "${CMAKE_BINARY_DIR}/${LIB_DIR}" ${lf} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
      elseif (APPLE)
	execute_process(COMMAND install_name_tool -delete_rpath "${BRLCAD_EXT_DIR}/extinstall/${LIB_DIR}" ${lf} WORKING_DIRECTORY ${CMAKE_BINARY_DIR} OUTPUT_VARIABLE OOUT RESULT_VARIABLE ORESULT ERROR_VARIABLE OERROR)
	execute_process(COMMAND install_name_tool -add_rpath "${CMAKE_BINARY_DIR}/${LIB_DIR}" ${lf} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
      endif (PATCHELF_EXECUTABLE)
      # RPATH updates are complete - now clear out any other stale paths in the file
      #message("${STRCLEAR_EXECUTABLE} -v -b -c ${CMAKE_BINARY_DIR}/${lf} ${BRLCAD_EXT_DIR_REAL}/${LIB_DIR} ${BRLCAD_EXT_DIR_REAL}/${BIN_DIR} ${BRLCAD_EXT_DIR_REAL}/${INCLUDE_DIR} ${BRLCAD_EXT_DIR_REAL}/")
      execute_process(COMMAND  ${STRCLEAR_EXECUTABLE} -v -b -c ${CMAKE_BINARY_DIR}/${lf} "${BRLCAD_EXT_DIR_REAL}/${LIB_DIR}" "${BRLCAD_EXT_DIR_REAL}/${BIN_DIR}" "${BRLCAD_EXT_DIR_REAL}/${INCLUDE_DIR}" "${BRLCAD_EXT_DIR_REAL}/")
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
	  execute_process(COMMAND install_name_tool -delete_rpath "${BRLCAD_EXT_DIR}/extinstall/${LIB_DIR}" ${lf} WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}" OUTPUT_VARIABLE OOUT RESULT_VARIABLE ORESULT ERROR_VARIABLE OERROR)
	  execute_process(COMMAND install_name_tool -add_rpath "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${LIB_DIR}" ${lf} WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
	endif (PATCHELF_EXECUTABLE)
	# RPATH updates are complete - now clear out any other stale paths in the file
	execute_process(COMMAND  ${STRCLEAR_EXECUTABLE} -v -b -c ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${lf} "${BRLCAD_EXT_DIR_REAL}/${LIB_DIR}" "${BRLCAD_EXT_DIR_REAL}/${BIN_DIR}" "${BRLCAD_EXT_DIR_REAL}/${INCLUDE_DIR}" "${BRLCAD_EXT_DIR_REAL}/")
      endforeach(lf ${BINARY_FILES})
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif (NOT CMAKE_CONFIGURATION_TYPES)
  message("Setting rpath on 3rd party lib and exe files... done.")


  message("Scrubbing paths from txt and data files...")
  # Also want to clear stale paths out of the files.
  foreach(tf ${NONEXEC_FILES})
    set(SKIP_FILE 0)
    foreach (skp ${NOPROCESS_PATTERNS})
      if ("${tf}" MATCHES "${skp}")
	set(SKIP_FILE 1)
	continue()
      endif ("${tf}" MATCHES "${skp}")
    endforeach (skp ${NOPROCESS_PATTERNS})
    if (SKIP_FILE)
      continue()
    endif (SKIP_FILE)

    # Replace any stale paths in the files
    #message("${STRCLEAR_EXECUTABLE} -v -b -c ${CMAKE_BINARY_DIR}/${tf} ${BRLCAD_EXT_DIR_REAL}")
    execute_process(COMMAND ${STRCLEAR_EXECUTABLE} -v -b -c "${CMAKE_BINARY_DIR}/${tf}" "${BRLCAD_EXT_DIR_REAL}")
  endforeach(tf ${NONEXEC_FILES})

  foreach(tf ${TEXT_FILES})
    if (IS_SYMLINK ${tf})
      continue()
    endif (IS_SYMLINK ${tf})
    set(SKIP_FILE 0)
    foreach (skp ${NOPROCESS_PATTERNS})
      if ("${tf}" MATCHES "${skp}")
	set(SKIP_FILE 1)
	continue()
      endif ("${tf}" MATCHES "${skp}")
    endforeach (skp ${NOPROCESS_PATTERNS})
    if (SKIP_FILE)
      continue()
    endif (SKIP_FILE)

    execute_process(COMMAND ${STRCLEAR_EXECUTABLE} -v -r "${CMAKE_BINARY_DIR}/${tf}" "${BRLCAD_EXT_DIR_REAL}" "${CMAKE_INSTALL_PREFIX}")
  endforeach(tf ${NONEXEC_FILES})
  message("Scrubbing paths from txt and data files... done.")

else (NOT EXISTS "${THIRDPARTY_INVENTORY}")

  file(STRINGS "${THIRDPARTY_INVENTORY}" THIRDPARTY_FILES)
  file(STRINGS "${THIRDPARTY_INVENTORY_BINARIES}" BINARY_FILES)

endif (NOT EXISTS "${THIRDPARTY_INVENTORY}")

foreach(tf ${THIRDPARTY_FILES})
  # Rather than doing the PROGRAMS install for all binary files, we target just
  # those in the bin directory - those are the ones we would expect to want
  # CMake's *_EXECUTE permissions during install.
  get_filename_component(dir "${tf}" DIRECTORY)
  if (NOT dir)
    message("Error - unexpected toplevel ext file: ${tf} ")
    continue()
  endif (NOT dir)
  # If we know it's a binary file, treat it accordingly
  if ("${tf}" IN_LIST BINARY_FILES)
    install(PROGRAMS "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
    continue()
  endif ("${tf}" IN_LIST BINARY_FILES)
  # BIN_DIR may contain scripts that aren't explicitly binary files -
  # catch those based on path
  if (${dir} MATCHES "${BIN_DIR}$")
    install(PROGRAMS "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
  else (${dir} MATCHES "${BIN_DIR}$")
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
  # Finalize the rpaths
  set(REL_RPATH)
  find_relative_rpath("${bf}" REL_RPATH)
  if (PATCHELF_EXECUTABLE)
    install(CODE "execute_process(COMMAND ${PATCHELF_EXECUTABLE} --remove-rpath \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
    install(CODE "execute_process(COMMAND ${PATCHELF_EXECUTABLE} --set-rpath \"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${REL_RPATH}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
  elseif (APPLE)
    install(CODE "execute_process(COMMAND install_name_tool -delete_rpath \"${CMAKE_BINARY_DIR}/${LIB_DIR}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\" OUTPUT_VARIABLE OOUT RESULT_VARIABLE ORESULT ERROR_VARIABLE OERROR)")
    install(CODE "execute_process(COMMAND install_name_tool -add_rpath \"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${REL_RPATH}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
  endif (PATCHELF_EXECUTABLE)
  # Overwrite any stale paths in the binary files with null chars, to make sure
  # they're not interfering with the behavior of the final executables.
  # TODO - this should be in two stages, or perhaps done just on copy.  We
  # should zap brlcad_external paths when doing the initial copy, and
  # CMAKE_BINARY_DIR paths (if any are needed) on install... that will
  # probably mean strclear belongs in brlcad_externals' extnoinstall bin
  # collection so it's available at configure time here...
  install(CODE "execute_process(COMMAND  ${STRCLEAR_EXECUTABLE} -v -b -c \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\" \"${CMAKE_BINARY_DIR}/${LIB_DIR}\")")
endforeach(bf ${BINARY_FILES})

# Because extinstall is handled at configure time (and indeed MUST be handled at
# configure time so find_package results will be correct) we make the CMake
# process depend on the extinstall files
foreach (ef ${THIRDPARTY_FILES})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BRLCAD_EXT_INSTALL_DIR}/${ef})
endforeach (ef ${THIRDPARTY_FILES})

# Not all packages will define all of these, but it shouldn't matter - an unset
# of an unused variable shouldn't be harmful
function(find_package_reset pname trigger_var)
  if (NOT ${trigger_var})
    return()
  endif (NOT ${trigger_var})
  unset(${pname}_DIR CACHE)
  unset(${pname}_CONFIG CACHE)
  unset(${pname}_DEFINITIONS CACHE)
  unset(${pname}_FOUND CACHE)
  unset(${pname}_INCLUDE_DIR CACHE)
  unset(${pname}_INCLUDE_DIRS CACHE)
  unset(${pname}_LIBRARIES CACHE)
  unset(${pname}_LIBRARY CACHE)
  unset(${pname}_LIBRARY_DEBUG CACHE)
  unset(${pname}_LIBRARY_RELEASE CACHE)
  unset(${pname}_VERSION_STRING CACHE)
  unset(${pname}_PREFIX_STR CACHE)
endfunction(find_package_reset pname trigger_var)

# zlib compression/decompression library
# https://zlib.net
#
# Note - our copy is modified from Vanilla upstream to support specifying a
# custom prefix - until a similar feature is available in upstream zlib, we
# need this to reliably avoid conflicts between bundled and system zlib.
find_package_reset(ZLIB RESET_THIRDPARTY)
set(ZLIB_ROOT "${CMAKE_BINARY_DIR}")
find_package(ZLIB REQUIRED)
if ("${ZLIB_LIBRARIES}" MATCHES "${CMAKE_BINARY_DIR}/.*")
  set(Z_PREFIX_STR "brl_" CACHE STRING "Using local zlib" FORCE)
endif ("${ZLIB_LIBRARIES}" MATCHES "${CMAKE_BINARY_DIR}/.*")

# libregex regular expression matching
find_package_reset(REGEX RESET_THIRDPARTY)
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
find_package_reset(NETPBM RESET_THIRDPARTY)
set(NETPBM_ROOT "${CMAKE_BINARY_DIR}")
find_package(NETPBM)

# libpng - Portable Network Graphics image file support
# http://www.libpng.org/pub/png/libpng.html
find_package_reset(PNG RESET_THIRDPARTY)
if (RESET_THIRDPARTY)
  unset(PNG_PNG_INCLUDE_DIR CACHE)
endif (RESET_THIRDPARTY)
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
  find_package_reset(STEPCODE RESET_THIRDPARTY)
  if (RESET_THIRDPARTY)
    unset(STEPCODE_CORE_LIBRARY    CACHE)
    unset(STEPCODE_DAI_DIR         CACHE)
    unset(STEPCODE_DAI_LIBRARY     CACHE)
    unset(STEPCODE_EDITOR_DIR      CACHE)
    unset(STEPCODE_EDITOR_LIBRARY  CACHE)
    unset(STEPCODE_EXPPP_DIR       CACHE)
    unset(STEPCODE_EXPPP_LIBRARY   CACHE)
    unset(STEPCODE_EXPRESS_DIR     CACHE)
    unset(STEPCODE_EXPRESS_LIBRARY CACHE)
    unset(STEPCODE_INCLUDE_DIR     CACHE)
    unset(STEPCODE_STEPCORE_DIR    CACHE)
    unset(STEPCODE_UTILS_DIR       CACHE)
    unset(STEPCODE_UTILS_LIBRARY   CACHE)
  endif (RESET_THIRDPARTY)
  set(STEPCODE_ROOT "${CMAKE_BINARY_DIR}")
  find_package(STEPCODE)
endif (BRLCAD_ENABLE_STEP)

# GDAL -  translator library for raster and vector geospatial data formats
# https://gdal.org
if (BRLCAD_ENABLE_GDAL)
  find_package_reset(GDAL RESET_THIRDPARTY)
  set(GDAL_ROOT "${CMAKE_BINARY_DIR}")
  find_package(GDAL)
endif (BRLCAD_ENABLE_GDAL)

# Open Asset Import Library - library for supporting I/O for a number of
# Geometry file formats
# https://github.com/assimp/assimp
if (BRLCAD_ENABLE_ASSETIMPORT)
  find_package_reset(ASSETIMPORT RESET_THIRDPARTY)
  set(ASSETIMPORT_ROOT "${CMAKE_BINARY_DIR}")
  find_package(ASSETIMPORT)
endif (BRLCAD_ENABLE_ASSETIMPORT)

# OpenMesh Library - library for representing and manipulating polygonal meshes
# https://www.graphics.rwth-aachen.de/software/openmesh/
if (BRLCAD_ENABLE_OPENMESH)
  find_package_reset(OPENMESH RESET_THIRDPARTY)
  if (RESET_THIRDPARTY)
    unset(OPENMESH_CORE_LIBRARY          CACHE)
    unset(OPENMESH_CORE_LIBRARY_DEBUG    CACHE)
    unset(OPENMESH_CORE_LIBRARY_RELEASE  CACHE)
    unset(OPENMESH_TOOLS_LIBRARY         CACHE)
    unset(OPENMESH_TOOLS_LIBRARY_DEBUG   CACHE)
    unset(OPENMESH_TOOLS_LIBRARY_RELEASE CACHE)
  endif (RESET_THIRDPARTY)
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

find_package_reset(TCL RESET_THIRDPARTY)
find_package_reset(TK RESET_THIRDPARTY)
if (RESET_THIRDPARTY)
  unset(TCL_INCLUDE_PATH CACHE)
  unset(TCL_STUB_LIBRARY CACHE)
  unset(TCL_TCLSH CACHE)
  unset(TK_INCLUDE_PATH CACHE)
  unset(TK_STUB_LIBRARY CACHE)
  unset(TK_WISH CACHE)
  unset(TK_X11_GRAPHICS CACHE)
  unset(TTK_STUB_LIBRARY CACHE)
endif (RESET_THIRDPARTY)

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

# Qt - cross-platform user interface/application development toolkit
# https://download.qt.io/archive/qt
if (BRLCAD_ENABLE_QT)

  find_package_reset(Qt5 RESET_THIRDPARTY)
  find_package_reset(Qt6 RESET_THIRDPARTY)

  set(QtComponents Core Widgets Gui Svg OpenGLWidgets Network)
  if (BRLCAD_ENABLE_OPENGL)
    set(QtComponents  ${QtComponents} OpenGL)
  endif (BRLCAD_ENABLE_OPENGL)

  if (RESET_THIRDPARTY)
    foreach(qc ${QtComponents})
      unset(Qt6${qc}_DIR CACHE)
      unset(Qt6${qc}_FOUND CACHE)
      unset(Qt5${qc}_DIR CACHE)
      unset(Qt5${qc}_FOUND CACHE)
    endforeach(qc ${QtComponents})
  endif (RESET_THIRDPARTY)

  # First, see if we have a bundled version
  set(Qt6_DIR_TMP "${Qt6_DIR}")
  set(Qt6_DIR "${CMAKE_BINARY_DIR}/${LIB_DIR}/cmake/Qt6")
  set(Qt6_ROOT ${CMAKE_BINARY_DIR})
  find_package(Qt6 COMPONENTS ${QtComponents})
  unset(Qt6_ROOT)

  if (NOT Qt6Widgets_FOUND)
    set(Qt6_DIR "${Qt6_DIR_TMP}")
    find_package(Qt6 COMPONENTS ${QtComponents})
  endif (NOT Qt6Widgets_FOUND)
  if (NOT Qt6Widgets_FOUND)
    # We didn't find 6, try 5.  For non-standard install locations, you may
    # need to set the following CMake variables:
    #
    # Qt5_DIR=<install_dir>/lib/cmake/Qt5
    # QT_QMAKE_EXECUTABLE=<install_dir>/bin/qmake
    # AUTORCC_EXECUTABLE=<install_dir>/bin/rcc
    find_package(Qt5 COMPONENTS ${QtComponents})
  endif (NOT Qt6Widgets_FOUND)
  if (NOT Qt6Widgets_FOUND AND NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
    message("Qt requested, but Qt installation not found - disabling")
    set(BRLCAD_ENABLE_QT OFF)
  endif (NOT Qt6Widgets_FOUND AND NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
  if (Qt6Widgets_FOUND)
    find_package(Qt6 COMPONENTS Test)
    if (Qt6Test_FOUND)
      CONFIG_H_APPEND(BRLCAD "#define USE_QTTEST 1\n")
    endif (Qt6Test_FOUND)
  endif (Qt6Widgets_FOUND)
  mark_as_advanced(Qt6Widgets_DIR)
  mark_as_advanced(Qt6Core_DIR)
  mark_as_advanced(Qt6Gui_DIR)
  mark_as_advanced(Qt5Widgets_DIR)
  mark_as_advanced(Qt5Core_DIR)
  mark_as_advanced(Qt5Gui_DIR)
endif (BRLCAD_ENABLE_QT)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

