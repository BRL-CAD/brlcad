#       B R L C A D _ E X T E R N A L D E P S . C M A K E
# BRL-CAD
#
# Copyright (c) 2023-2024 United States Government as represented by
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

# When we need to have CMake treat includes as system paths to avoid warnings,
# we add those patterns to the SYS_INCLUDE_PATTERNS list
mark_as_advanced(SYS_INCLUDE_PATTERNS)

if (NOT EXISTS "${BRLCAD_EXT_INSTALL_DIR}" OR NOT EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}")
  message("Attempting to prepare our own version of the bext dependencies\n")
  include(BRLCAD_EXT_Setup)
  brlcad_ext_setup()
endif ()

# If we got to ${BRLCAD_EXT_DIR}/install through a symlink, we need to expand it so
# we can spot the path that would have been used in ${BRLCAD_EXT_DIR}/install files
#
# TODO - once we can require CMake 3.21 minimum, add EXPAND_TILDE to
# the arguments list
file(REAL_PATH "${BRLCAD_EXT_INSTALL_DIR}" BRLCAD_EXT_DIR_REAL)


# See if we have plief available for rpath manipulation.  If it is available,
# we will be using it to manage the RPATH settings for third party exe/lib
# files.  If not, see if patchelf is available instead.
find_program(P_RPATH_EXECUTABLE NAMES plief HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})
if (NOT P_RPATH_EXECUTABLE)
  find_program(P_RPATH_EXECUTABLE NAMES patchelf HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})
endif (NOT P_RPATH_EXECUTABLE)

# Find the tool we use to scrub EXT paths from files
find_program(STRCLEAR_EXECUTABLE strclear HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})


# For repeat configure passes, we need to check any existing files copied
# against the ${BRLCAD_EXT_DIR}/install dir's contents, to detect if the latter has changed
# and we need to redo the process.
set(TP_INVENTORY "${CMAKE_BINARY_DIR}/CMakeFiles/thirdparty.txt")
set(TP_INVENTORY_BINARIES "${CMAKE_BINARY_DIR}/CMakeFiles/thirdparty_binaries.txt")


# These patterns are used to identify sets of files where we are assuming we
# don't need to do post-processing to correct file paths from the external
# install.
set(NOPROCESS_PATTERNS
  ".*/encodings/.*"
  ".*/include/.*"
  ".*/man/.*"
  ".*/msgs/.*"
  )

# These patterns are to be excluded from ${BRLCAD_EXT_DIR}/install bundling - i.e., even if
# present in the specified ${BRLCAD_EXT_DIR}/install directory, BRL-CAD will not incorporate
# them.  Generally speaking this is used to avoid files needed for external
# building but counterproductive in the BRL-CAD install.
set(EXCLUDED_PATTERNS
  ${LIB_DIR}/itcl4.2.3/itclConfig.sh
  ${LIB_DIR}/tclConfig.sh
  ${LIB_DIR}/tdbc1.1.5/tdbcConfig.sh
  ${LIB_DIR}/tkConfig.sh
  )


#####################################################################
# Utility functions for use when processing ${BRLCAD_EXT_DIR}/install files
#####################################################################


# In multiconfig we need to scrub excluded files out of multiple ${BRLCAD_EXT_DIR}/install
# copies, so wrap logic to do so into a function.
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



# See if a file matches a pattern to skip its processing
# Sets the variable held in SVAR in the parent scope
function(SKIP_PROCESSING tf SVAR)
  if (IS_SYMLINK ${tf})
    set(${SVAR} 1 PARENT_SCOPE)
    return()
  endif (IS_SYMLINK ${tf})
  foreach (skp ${NOPROCESS_PATTERNS})
    if ("${tf}" MATCHES "${skp}")
      set(${SVAR} 1 PARENT_SCOPE)
      return()
    endif ("${tf}" MATCHES "${skp}")
  endforeach (skp ${NOPROCESS_PATTERNS})
  set(${SVAR} 0 PARENT_SCOPE)
endfunction(SKIP_PROCESSING)


# Since the checking process can be long, we want some sort
# of feedback indicating we're progressing
function(BFILE_TYPE_MSG ALL_CNT ALL_PROCESSED BINARY_LIST)
  list(LENGTH ${BINARY_LIST} BCNT)
  if (BCNT)
    math(EXPR skip_msg  "${BCNT} % 100")
    if (${skip_msg} EQUAL 0)
      message("Found ${BCNT} shared object or executable files (characterized ${ALL_PROCESSED} of ${ALL_CNT} files.")
      return()
    endif (${skip_msg} EQUAL 0)
  endif (BCNT)
endfunction(BFILE_TYPE_MSG)

function(NFILE_TYPE_MSG ALL_CNT ALL_PROCESSED NOEXEC_LIST)
  list(LENGTH ${NOEXEC_LIST} NCNT)
  if (NCNT)
    math(EXPR skip_msg  "${NCNT} % 100")
    if (${skip_msg} EQUAL 0)
      message("Found ${NCNT} binary files (characterized ${ALL_PROCESSED} of ${ALL_CNT} files.")
      return()
    endif (${skip_msg} EQUAL 0)
  endif (NCNT)
endfunction(NFILE_TYPE_MSG)

function(TFILE_TYPE_MSG ALL_CNT ALL_PROCESSED TEXT_LIST)
  list(LENGTH ${TEXT_LIST} TCNT)
  if (TCNT)
    math(EXPR skip_msg  "${TCNT} % 500")
    if (${skip_msg} EQUAL 0)
      message("Found ${TCNT} text files (characterized ${ALL_PROCESSED} of ${ALL_CNT} files.")
      return()
    endif (${skip_msg} EQUAL 0)
  endif (TCNT)
endfunction(TFILE_TYPE_MSG)

# For processing purposes, there are three categories of ${BRLCAD_EXT_DIR}/install file:
#
# 1.  exe or shared library files needing RPATH adjustment (which also need
#     to be installed with executable permissions)
# 2.  binary files which are NOT executable files (images, etc.)
# 3.  text files
#
# If we don't want to hard-code information about specific files we expect
# to find in ${BRLCAD_EXT_DIR}/install, we need a way to detect "on the fly" what we are
# dealing with.
function(FILE_TYPE fname ALL_CNT BINARY_LIST TEXT_LIST NOEXEC_LIST)
  if (IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})
    return()
  endif (IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})

  list(LENGTH ${BINARY_LIST} BCNT)
  list(LENGTH ${TEXT_LIST} TCNT)
  list(LENGTH ${NOEXEC_LIST} NCNT)
  math(EXPR PCNT "${BCNT} + ${TCNT} + ${NCNT} + 1")

  foreach (skp ${NOPROCESS_PATTERNS})
    if ("${fname}" MATCHES "${skp}")
      set(${TEXT_LIST} ${${TEXT_LIST}} ${fname} PARENT_SCOPE)
      TFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${TEXT_LIST})
      return()
    endif ("${fname}" MATCHES "${skp}")
  endforeach (skp ${NOPROCESS_PATTERNS})
  execute_process(COMMAND ${STRCLEAR_EXECUTABLE} -B ${CMAKE_BINARY_DIR}/${fname} RESULT_VARIABLE TXT_FILE)
  if ("${TXT_FILE}" GREATER 0)
    set(${TEXT_LIST} ${${TEXT_LIST}} ${fname} PARENT_SCOPE)
    TFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${TEXT_LIST})
    return()
  endif ("${TXT_FILE}" GREATER 0)

  # Some kind of binary file, can we set an RPATH?
  if (P_RPATH_EXECUTABLE)
    execute_process(COMMAND ${P_RPATH_EXECUTABLE} ${CMAKE_BINARY_DIR}/${lf} RESULT_VARIABLE NOT_BIN_OBJ OUTPUT_VARIABLE NB_OUT ERROR_VARIABLE NB_ERR)
    if (NOT_BIN_OBJ)
      set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
      NFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${NOEXEC_LIST})
      return()
    else (NOT_BIN_OBJ)
      #message("File needing RPATH setting: ${fname}")
      set(${BINARY_LIST} ${${BINARY_LIST}} ${fname} PARENT_SCOPE)
      BFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${BINARY_LIST})
      return()
    endif (NOT_BIN_OBJ)
  endif(P_RPATH_EXECUTABLE)
  if (APPLE)
    execute_process(COMMAND otool -l ${CMAKE_BINARY_DIR}/${lf} RESULT_VARIABLE ORESULT OUTPUT_VARIABLE OTOOL_OUT ERROR_VARIABLE NB_ERR)
    if ("${OTOOL_OUT}" MATCHES "Archive")
      set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
      NFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${NOEXEC_LIST})
      return()
    endif ("${OTOOL_OUT}" MATCHES "Archive")
    if ("${OTOOL_OUT}" MATCHES "not an object")
      set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
      NFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${NOEXEC_LIST})
      return()
    endif ("${OTOOL_OUT}" MATCHES "not an object")
    # Not one of the exceptions - binary
    #message("File needing RPATH setting: ${fname}")
    set(${BINARY_LIST} ${${BINARY_LIST}} ${fname} PARENT_SCOPE)
    BFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${BINARY_LIST})
    return()
  endif(APPLE)

  # If we haven't figured it out, treat as noexec binary
  set(${NOEXEC_LIST} ${${NOEXEC_LIST}} ${fname} PARENT_SCOPE)
  NFILE_TYPE_MSG(${ALL_CNT} ${PCNT} ${NOEXEC_LIST})
endfunction(FILE_TYPE)



# Copy everything in ${BRLCAD_EXT_DIR}/install into the build directory
function(INITIALIZE_TP_FILES)
  # Rather than complicate matters trying to pick and choose what to move, just
  # stage everything.  Depending on what the dependencies write into their
  # install directories we may have to be more selective about this in the
  # future, but for now let's try simplicity - the less we can couple this
  # logic to the specific contents of ${BRLCAD_EXT_DIR}/install, the better.
  #
  # Unfortuately, as implemented this is currently quite slow, but cmake's
  # -E copy_directory command follows symlinks rather than duplicating them:
  # https://cmake.org/cmake/help/latest/manual/cmake.1.html
  if ("${BRLCAD_EXT_DIR}/install" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
    set(EXT_DIR_STR "${BRLCAD_EXT_INSTALL_DIR}")
  else ("${BRLCAD_EXT_DIR}/install" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
    set(EXT_DIR_STR "${BRLCAD_EXT_INSTALL_DIR} (via symlink ${BRLCAD_EXT_DIR}/install)")
  endif ("${BRLCAD_EXT_DIR}/install" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
  message("Staging third party files from ${EXT_DIR_STR} in build directory...")
  file(GLOB SDIRS LIST_DIRECTORIES true RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
  foreach(sd ${SDIRS})
    # Bundled up the sub-directory's contents so that the archive will expand with
    # paths relative to the ${BRLCAD_EXT_DIR}/install root
    message("Packing ${sd} subdirectory...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/${sd}.tar "${BRLCAD_EXT_INSTALL_DIR}/${sd}" WORKING_DIRECTORY "${BRLCAD_EXT_INSTALL_DIR}")
    message("Packing ${sd} subdirectory... done.")

    # Make sure the build directory has the target directory to write to
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${sd})

    # Whether we're single or multi config, do a top level decompression to give
    # the install targets a uniform source for all configurations.
    message("Expanding ${sd}.tar in build directory...")
    if ("${CMAKE_VERSION}" VERSION_LESS "3.24")
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    else ("${CMAKE_VERSION}" VERSION_LESS "3.24")
      # If we have it, use --touch instead of the (very slow) per file -E touch update
      # https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-E_tar-touch
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar --touch WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    endif ("${CMAKE_VERSION}" VERSION_LESS "3.24")
    message("Expanding ${sd}.tar in build directory... done.")

    # For multi-config, we'll also need to decompress once for each active configuration's build dir
    # so the executables will work locally...
    if (CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${sd})
	message("Expanding ${sd}.tar in configuration: ${CFG_TYPE} build directory...")
	if ("${CMAKE_VERSION}" VERSION_LESS "3.24")
	  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
	else ("${CMAKE_VERSION}" VERSION_LESS "3.24")
	  # If we have it, use --touch instead of the (very slow) per file -E touch update
	  # https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-E_tar-touch
	  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar --touch WORKING_DIRECTORY "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
	endif ("${CMAKE_VERSION}" VERSION_LESS "3.24")
	message("Expanding ${sd}.tar in configuration: ${CFG_TYPE} build directory... done.")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif (CMAKE_CONFIGURATION_TYPES)

    # Copying complete - remove the archive file
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${sd}.tar)
  endforeach(sd ${SDIRS})

  # The above copy is indiscriminate, so we follow behind it and strip out the
  # files we don't wish to include
  message("Removing files indicated by exclude patterns...")
  STRIP_EXCLUDED("${CMAKE_BINARY_DIR}" EXCLUDED_PATTERNS)
  if (CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      STRIP_EXCLUDED("${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}" EXCLUDED_PATTERNS)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif (CMAKE_CONFIGURATION_TYPES)
  message("Removing files indicated by exclude patterns... done.")

  # In older CMake, unpacking the files didn't come with the option to update
  # their timestamps - see https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-E_tar-touch
  # If we want to be able to check if ${BRLCAD_EXT_DIR}/install has changed, we need to make
  # sure the build dir copies are newer than the ${BRLCAD_EXT_DIR}/install copies for
  # IS_NEWER_THAN testing.
  if ("${CMAKE_VERSION}" VERSION_LESS "3.24")
    message("Updating copied file timestamps...")
    foreach(tf ${TP_FILES})
      execute_process(COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_BINARY_DIR}/${tf}")
    endforeach(tf ${TP_FILES})
    message("Updating copied file timestamps... done.")
  endif ("${CMAKE_VERSION}" VERSION_LESS "3.24")

  message("Staging third party files from ${EXT_DIR_STR} in build directory... done.")

  # NOTE - we may need to find and redo symlinks, if we get any that are full
  # paths - we expect .so and .so.* style symlinks on some platforms, and there
  # may be others.  If those paths are absolute in BRLCAD_EXT_INSTALL_DIR they will be
  # wrong when copied into the BRL-CAD build - they will work on the build
  # machine since full path links will resolve, but will fail when installed on
  # another machine.  A quick tests suggests we don't have any like that right
  # now, but it's not clear we can count on that...
endfunction(INITIALIZE_TP_FILES)



# If we have a pre-existing list of files, we need to determine the status of the
# current directories vs the list.  Sets three lists at the parent scope:
# TP_NEW     - files in ${BRLCAD_EXT_DIR}/install that are new since the previous list was generated
# TP_STALE   - files in the old list that are no longer in ${BRLCAD_EXT_DIR}/install
# TP_CHANGED - files present in both lists but newer in ${BRLCAD_EXT_DIR}/install
#
# Note that TP_NEW will be empty if there is no previous state.  The main logic
# uses a different variable - TP_INIT - to notify the appropriate processing steps
# that there are files to work on, since we don't want to do the copying step
# with configure_file when the initialize routines have already done the work.
function(TP_COMPARE_STATE TP_NEW_LIST TP_PREV_LIST)

  # See if any new files have appeared compared to the previous state
  set(LTP_NEW "${${TP_NEW_LIST}}")
  set(LTP_PREVIOUS "${${TP_PREV_LIST}}")
  if (LTP_PREVIOUS)
    list(REMOVE_ITEM LTP_NEW ${LTP_PREVIOUS})
  else (LTP_PREVIOUS)
    # If everything is new, we're initializing
    set(LTP_NEW)
  endif (LTP_PREVIOUS)

  # See if any files previously copied into the build dir have been removed
  set(LTP_STALE ${LTP_PREVIOUS})
  if (${TP_NEW_LIST})
    list(REMOVE_ITEM LTP_STALE ${${TP_NEW_LIST}})
  endif (${TP_NEW_LIST})

  # We also need to see if any files are new based on timestamps.
  set(LTP_CHANGED)
  set(LTP_EXISTING ${${TP_NEW_LIST}})
  if (LTP_NEW)
    list(REMOVE_ITEM LTP_EXISTING ${LTP_NEW})
  endif (LTP_NEW)
  foreach (ef ${LTP_EXISTING})
    if (${BRLCAD_EXT_INSTALL_DIR}/${ef} IS_NEWER_THAN ${CMAKE_BINARY_DIR}/${ef})
      set(LTP_CHANGED ${LTP_CHANGED} ${ef})
    endif (${BRLCAD_EXT_INSTALL_DIR}/${ef} IS_NEWER_THAN ${CMAKE_BINARY_DIR}/${ef})
  endforeach (ef ${LTP_EXISTING})

  set(TP_NEW "${LTP_NEW}" PARENT_SCOPE)
  set(TP_STALE "${LTP_STALE}" PARENT_SCOPE)
  set(TP_CHANGED "${LTP_CHANGED}" PARENT_SCOPE)

endfunction(TP_COMPARE_STATE)



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
    set(RELATIVE_RPATH "@executable_path")
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



# Apply the RPATH settings to be used in the build directory.  This is a bit
# different from what is done for the final install - the goal here is not
# to produce relocatable files, but just have things work in place in the build
# locations.  Parameterized to allow processing of both single and multiconfig
# builds.
function(RPATH_BUILD_DIR_PROCESS ROOT_DIR lf)
  if (P_RPATH_EXECUTABLE)
    execute_process(COMMAND ${P_RPATH_EXECUTABLE} --set-rpath "${ROOT_DIR}/${LIB_DIR}" ${lf} WORKING_DIRECTORY ${ROOT_DIR})
  elseif (APPLE)
    execute_process(COMMAND install_name_tool -delete_rpath "${BRLCAD_EXT_DIR}/${BRLCAD_EXT_DIR}/install/${LIB_DIR}" ${lf} WORKING_DIRECTORY ${ROOT_DIR} OUTPUT_VARIABLE OOUT RESULT_VARIABLE ORESULT ERROR_VARIABLE OERROR)
    execute_process(COMMAND install_name_tool -add_rpath "${ROOT_DIR}/${LIB_DIR}" ${lf} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  endif (P_RPATH_EXECUTABLE)
  # RPATH updates are complete - now clear out any other stale paths in the file
  execute_process(COMMAND  ${STRCLEAR_EXECUTABLE} -v -b -c ${ROOT_DIR}/${lf} "${BRLCAD_EXT_DIR_REAL}/${LIB_DIR}" "${BRLCAD_EXT_DIR_REAL}/${BIN_DIR}" "${BRLCAD_EXT_DIR_REAL}/${INCLUDE_DIR}" "${BRLCAD_EXT_DIR_REAL}/")
  # Modern Apple security features (particularly on ARM64) complicate our manipulation of these
  # files in this fashion.  For more info, see:
  # https://developer.apple.com/documentation/security/updating_mac_software
  # https://developer.apple.com/documentation/xcode/embedding-nonstandard-code-structures-in-a-bundle
  # https://stackoverflow.com/questions/71744856/install-name-tool-errors-on-arm64
  if (APPLE)
    execute_process(COMMAND codesign --force -s - ${lf} WORKING_DIRECTORY ${ROOT_DIR})
  endif (APPLE)
endfunction(RPATH_BUILD_DIR_PROCESS)




#####################################################################
# Start of processing for BRLCAD_EXT_INSTALL_DIR contents
# We need to keep the build directory copies of ${BRLCAD_EXT_DIR}/install files in
# sync with the BRLCAD_EXT_DIR originals, if they change.
#####################################################################

# Ascertain the current state of ${BRLCAD_EXT_DIR}/install
file(GLOB_RECURSE TP_FILES LIST_DIRECTORIES false RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
# Filter out the files removed via STRIP_EXCLUDED
foreach(ep ${EXCLUDED_PATTERNS})
  list(FILTER TP_FILES EXCLUDE REGEX ${ep})
endforeach(ep ${EXCLUDED_PATTERNS})


# For the very first pass w bulk copy the contents of the
# BRLCAD_EXT_INSTALL_DIR tree into our own directory.  For some of the
# external dependencies (like Tcl) library elements must be in sane relative
# locations to binaries being executed, and leaving them in
# BRLCAD_EXT_INSTALL_DIR won't work.  On Windows, the dlls for all the
# dependencies will need to be located correctly relative to the bin build
# directory.
set(TP_INIT)
if (NOT EXISTS "${TP_INVENTORY}")

  INITIALIZE_TP_FILES()

  # Special variable for when we need to know about first time initialization
  set(TP_INIT "${TP_FILES}")

  # With a clean copy, there aren't any previous files to check
  set(TP_PREVIOUS)

else (NOT EXISTS "${TP_INVENTORY}")

  # If we are repeating a configure process, we need to see what (if anything)
  # has changed.  Read in the previous list.
  file(READ "${TP_INVENTORY}" TP_P)
  string(REPLACE "\n" ";" TP_PREVIOUS "${TP_P}")

endif (NOT EXISTS "${TP_INVENTORY}")

# Write the current third party file list
string(REPLACE ";" "\n" TP_W "${TP_FILES}")
file(WRITE "${TP_INVENTORY}" "${TP_W}")

# Make sure both lists are sorted
list(SORT TP_FILES)
list(SORT TP_PREVIOUS)

# See what the delta looks like between the previous ${BRLCAD_EXT_DIR}/install state (if any)
# and the current
message("Comparing previous and current states...")
TP_COMPARE_STATE(TP_FILES TP_PREVIOUS)
message("Comparing previous and current states... done.")

# If we do have changes in a repeat configure process, we're going to have to
# redo the find_package tests.  However, we don't want to repeat them if we
# don't have to, so key the reset process on what we find.
set(RESET_TP FALSE CACHE BOOL "resetting flag")

if (BRLCAD_TP_FULL_RESET)

  if (TP_NEW OR TP_CHANGED OR TP_STALE)

    # If the user has requested it, if anything has changed we do a full flush
    # and re-copy of the ${BRLCAD_EXT_DIR}/install contents.  This is useful if one is changing
    # the ${BRLCAD_EXT_DIR}/install directory to a completely different directory rather than
    # incrementally updating the same directory.  In the former case, timestamps
    # aren't a reliable indicator of what to update in the build tree.
    #
    # The tradeoff is a full re-initialization is usually slower, since it is
    # doing more work.  On platforms where configure is slow, this can be
    # significant.  Hence the user setting to control behavior.

    # Clear old files
    foreach (ef ${TP_PREVIOUS})
      file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
      if (CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  file(REMOVE ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef})
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif (CMAKE_CONFIGURATION_TYPES)
    endforeach (ef ${TP_PREVIOUS})

    # Redo full copy
    INTIIALIZE_TP_FILES()

    # Reset all the find_package results
    set(RESET_TP TRUE CACHE BOOL "resetting flag")

  endif (TP_NEW OR TP_CHANGED OR TP_STALE)

else (BRLCAD_TP_FULL_RESET)

  # Clear copies of anything found to be stale
  if (TP_STALE)
    message("Removing stale 3rd party files in build directory...")
    foreach (ef ${TP_STALE})
      file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
      message("  ${CMAKE_BINARY_DIR}/${ef}")
      if (CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  file(REMOVE ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef})
	  message("  ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef}")
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif (CMAKE_CONFIGURATION_TYPES)
    endforeach (ef ${TP_STALE})
    message("Removing stale 3rd party files in build directory... done.")
  endif (TP_STALE)

  # Stage new files - we don't have the bulk tar mechanism going after the first
  # configure pass, so we have to do the copies needed explicitly.  TP_COMPARE_STATE
  # shouldn't populate TP_NEW unless we have a previous state to compare to.
  #
  # configure_file follows symlinks rather than copying them, so we can't use
  # that for this application and have to fall back on file(COPY):
  # https://gitlab.kitware.com/cmake/cmake/-/issues/14609
  if (TP_NEW)
    message("Staging new 3rd party files from ${BRLCAD_EXT_DIR}/install...")
    foreach (ef ${TP_NEW})
      file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
      get_filename_component(EF_DIR ${ef} DIRECTORY)
      get_filename_component(EF_NAME ${ef} NAME)
      file(COPY ${BRLCAD_EXT_INSTALL_DIR}/${ef} DESTINATION ${CMAKE_BINARY_DIR}/${EF_DIR})
      message("  ${CMAKE_BINARY_DIR}/${ef}")
      if (CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  file(REMOVE ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef})
	  file(COPY ${BRLCAD_EXT_INSTALL_DIR}/${ef} DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${EF_DIR})
	  message("  ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef}")
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif (CMAKE_CONFIGURATION_TYPES)
    endforeach (ef ${TP_CHANGED})
    message("Staging new 3rd party files from ${BRLCAD_EXT_DIR}/install... done.")
  endif (TP_NEW)

  # Stage changed files
  if (TP_CHANGED)
    message("Staging changed 3rd party files from ${BRLCAD_EXT_DIR}/install...")
    foreach (ef ${TP_CHANGED})
      file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
      get_filename_component(EF_DIR ${ef} DIRECTORY)
      get_filename_component(EF_NAME ${ef} NAME)
      file(COPY ${BRLCAD_EXT_INSTALL_DIR}/${ef} DESTINATION ${CMAKE_BINARY_DIR}/${EF_DIR})
      execute_process(COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_BINARY_DIR}/${ef}")
      message("  ${CMAKE_BINARY_DIR}/${ef}")
      if (CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  file(REMOVE ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef})
	  file(COPY ${BRLCAD_EXT_INSTALL_DIR}/${ef} DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${EF_DIR})
	  execute_process(COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef}")
	  message("  ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${ef}")
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif (CMAKE_CONFIGURATION_TYPES)
    endforeach (ef ${TP_CHANGED})
    message("Staging changed 3rd party files from ${BRLCAD_EXT_DIR}/install... done.")
  endif (TP_CHANGED)

  # If the directory file lists differ, we have to reset find package
  if (TP_NEW OR TP_STALE OR TP_CHANGED)
    set(RESET_TP TRUE CACHE BOOL "resetting flag")
  endif (TP_NEW OR TP_STALE OR TP_CHANGED)

endif (BRLCAD_TP_FULL_RESET)

# We'll either have TP_INIT (on the first pass) or (possibly) one or both of the
# others.  Regardless, the processing from here on out is the same.
set(TP_PROCESS ${TP_CHANGED} ${TP_NEW} ${TP_INIT})

# We're only going to characterize new files, but even on repeat configures
# we need to know about ALL binary files, old and new, for defining the
# install rules.  Read the cached list, if any, and scrub out any entries
# that are in one of the processing or clean-up lists
set(BINARY_FILES)
set(TEXT_FILES)
set(NOEXEC_FILES)
if (EXISTS ${TP_INVENTORY_BINARIES})
  file(READ "${TP_INVENTORY_BINARIES}" TP_B)
  string(REPLACE "\n" ";" BINARY_FILES "${TP_B}")
  if (TP_STALE)
    list(REMOVE_ITEM BINARY_FILES ${TP_STALE})
  endif (TP_STALE)
  if (TP_NEW)
    list(REMOVE_ITEM BINARY_FILES ${TP_NEW})
  endif (TP_NEW)
  if (TP_CHANGED)
    list(REMOVE_ITEM BINARY_FILES ${TP_CHANGED})
  endif (TP_CHANGED)
endif (EXISTS ${TP_INVENTORY_BINARIES})

# Use various tools to sort out which files are exec/lib files,
# targeting only the files we've determined need processing (for
# an initialization this is everything, but for subsequent passes
# there is likely to be much less work to do.)
message("Characterizing new or changed bundled third party files...")
set(NBINARY_FILES)
set(NTEXT_FILES)
set(NNOEXEC_FILES)
list(LENGTH TP_PROCESS ALL_PCNT)
foreach(lf ${TP_PROCESS})
  FILE_TYPE("${lf}" ${ALL_PCNT} NBINARY_FILES NTEXT_FILES NNOEXEC_FILES)
endforeach(lf ${TP_PROCESS})
message("Characterizing new or changed bundled third party files... done.")

# Combine the previous lists and the new determinations, writing
# the final lists back out to files
set(ALL_BINARY_FILES ${BINARY_FILES} ${NBINARY_FILES})
string(REPLACE ";" "\n" TP_B "${ALL_BINARY_FILES}")
file(WRITE "${TP_INVENTORY_BINARIES}" "${TP_B}")

if (NBINARY_FILES)
  message("Setting rpath on new 3rd party lib and exe files...")
  if (NOT CMAKE_CONFIGURATION_TYPES)
    # Set local RPATH so the files will work during build
    foreach(lf ${NBINARY_FILES})
      RPATH_BUILD_DIR_PROCESS("${CMAKE_BINARY_DIR}" "${lf}")
    endforeach(lf ${NBINARY_FILES})
  else (NOT CMAKE_CONFIGURATION_TYPES)
    # For multi-config, we set the RPATHs for each active configuration's build dir
    # so the executables will work locally.  We don't need to set the top level copy
    # being used for the install target since in multi-config those copies won't be
    # used by build directory executables
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      foreach(lf ${NBINARY_FILES})
	RPATH_BUILD_DIR_PROCESS("${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}" "${lf}")
      endforeach(lf ${NBINARY_FILES})
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif (NOT CMAKE_CONFIGURATION_TYPES)
  message("Setting rpath on new 3rd party lib and exe files... done.")
endif (NBINARY_FILES)

if (NNOEXEC_FILES)
  message("Scrubbing paths from new 3rd party data files...")
  foreach(tf ${NNOEXEC_FILES})
    SKIP_PROCESSING(${tf} SKIP_FILE)
    if (SKIP_FILE)
      continue()
    endif (SKIP_FILE)

    # Replace any stale paths in the files
    #message("${STRCLEAR_EXECUTABLE} -v -b -c ${CMAKE_BINARY_DIR}/${tf} ${BRLCAD_EXT_DIR_REAL}")
    execute_process(COMMAND ${STRCLEAR_EXECUTABLE} -v -b -c "${CMAKE_BINARY_DIR}/${tf}" "${BRLCAD_EXT_DIR_REAL}")
  endforeach(tf ${NNOEXEC_FILES})
  message("Scrubbing paths from new 3rd party data files... done.")
endif (NNOEXEC_FILES)

if (NTEXT_FILES)
  message("Replacing paths in new 3rd party text files...")
  foreach(tf ${NTEXT_FILES})
    SKIP_PROCESSING(${tf} SKIP_FILE)
    if (SKIP_FILE)
      continue()
    endif (SKIP_FILE)

    execute_process(COMMAND ${STRCLEAR_EXECUTABLE} -v -r "${CMAKE_BINARY_DIR}/${tf}" "${BRLCAD_EXT_DIR_REAL}" "${CMAKE_INSTALL_PREFIX}")
  endforeach(tf ${NTEXT_FILES})
  message("Replacing paths in new 3rd party text files... done.")
endif (NTEXT_FILES)

# Everything until now has been setting the stage in the build directory. Now
# we set up the install rules.  It is for these stages that we need complete
# knowledge of the third party files, since configure re-defines all of these
# rules on every pass.
foreach(tf ${TP_FILES})
  # Rather than doing the PROGRAMS install for all binary files, we target just
  # those in the bin directory - those are the ones we would expect to want
  # CMake's *_EXECUTE permissions during install.
  get_filename_component(dir "${tf}" DIRECTORY)
  if (NOT dir)
    message("Error - unexpected toplevel ext file: ${tf} ")
    continue()
  endif (NOT dir)
  # If we know it's a binary file, treat it accordingly
  if ("${tf}" IN_LIST ALL_BINARY_FILES)
    install(PROGRAMS "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
    continue()
  endif ("${tf}" IN_LIST ALL_BINARY_FILES)
  # BIN_DIR may contain scripts that aren't explicitly binary files -
  # catch those based on path
  if (${dir} MATCHES "${BIN_DIR}$")
    install(PROGRAMS "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
  else (${dir} MATCHES "${BIN_DIR}$")
    install(FILES "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
  endif (${dir} MATCHES "${BIN_DIR}$")
endforeach(tf ${TP_FILES})

# When installing, need to fix the RPATH on binary files again, similarly to
# what we did when staging in the build directory.  Again we don't process
# symlinks since following them will just result in re-processing the same
# file's RPATH multiple times.  This time, in contrast to the build directory
# setup, our goal is to define an RPATH that will allow the binary files to
# work when the install directory is relocated.
foreach(bf ${ALL_BINARY_FILES})
  if (IS_SYMLINK ${bf})
    continue()
  endif (IS_SYMLINK ${bf})
  # Finalize the rpaths
  set(REL_RPATH)
  find_relative_rpath("${bf}" REL_RPATH)
  if (P_RPATH_EXECUTABLE)
    install(CODE "execute_process(COMMAND ${P_RPATH_EXECUTABLE} --set-rpath \"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${REL_RPATH}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
  elseif (APPLE)
    install(CODE "execute_process(COMMAND install_name_tool -delete_rpath \"${CMAKE_BINARY_DIR}/${LIB_DIR}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\" OUTPUT_VARIABLE OOUT RESULT_VARIABLE ORESULT ERROR_VARIABLE OERROR)")
    install(CODE "execute_process(COMMAND install_name_tool -add_rpath \"${REL_RPATH}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
  endif (P_RPATH_EXECUTABLE)
  # Overwrite any stale paths in the binary files with null chars, to make sure
  # they're not interfering with the behavior of the final executables.  This
  # is a little fraught in that there's no guarantee these changes aren't going
  # to break something, but given that reliance on invalid full paths was going
  # to break something in any case eventually doing this will let us find out
  # about it sooner.  If the path is just a stale, unused leftover this should
  # have no impact on functionality, and otherwise this offers a way to avoid
  # "accidental success" where the program is using a build dir file to
  # successfully run when we don't want it to see them.
  install(CODE "execute_process(COMMAND  ${STRCLEAR_EXECUTABLE} -v -b -c \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\" \"${CMAKE_BINARY_DIR}/${LIB_DIR}\")")
  if (APPLE)
    # As with the configure time processing, use codesign at install time to appease OSX:
    # https://developer.apple.com/documentation/security/updating_mac_software
    # https://developer.apple.com/documentation/xcode/embedding-nonstandard-code-structures-in-a-bundle
    # https://stackoverflow.com/questions/71744856/install-name-tool-errors-on-arm64
    install(CODE "execute_process(COMMAND codesign --force -s - \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${bf}\")")
  endif (APPLE)
endforeach(bf ${ALL_BINARY_FILES})

# Because ${BRLCAD_EXT_DIR}/install is handled at configure time (and indeed MUST be handled at
# configure time so find_package results will be correct) we make the CMake
# process depend on the ${BRLCAD_EXT_DIR}/install files
foreach (ef ${TP_FILES})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BRLCAD_EXT_INSTALL_DIR}/${ef})
endforeach (ef ${TP_FILES})

# Add a extnoinstall touched file to also trigger CMake as above, to help
# ensure a reconfigure whenever the brlcad_externals repository is built.
# There should be a build-stamp file there that should be updated after each
# build run in brlcad_externals, regardless of what happens with other files.
file(GLOB_RECURSE TP_NOINST_FILES LIST_DIRECTORIES false RELATIVE "${BRLCAD_EXT_NOINSTALL_DIR}" "${BRLCAD_EXT_NOINSTALL_DIR}/*")
# For consistency, ignore files that would fall into the STRIP_EXCLUDED set
foreach(ep ${EXCLUDED_PATTERNS})
  list(FILTER TP_NOINST_FILES EXCLUDE REGEX ${ep})
endforeach(ep ${EXCLUDED_PATTERNS})
foreach (ef ${TP_NOINST_FILES})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BRLCAD_EXT_NOINSTALL_DIR}/${ef})
endforeach (ef ${TP_NOINST_FILES})


#####################################################################
# We want find_package calls that re-run every time configure is
# run, which means we need to unset cache variables.  Most of the
# packages use the BRLCAD_Find_Package wrapper for this, but in a
# few cases it's more complicated.
#####################################################################

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
macro(find_package_zlib)

  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(ZLIB RESET_TP)
  unset(Z_PREFIX CACHE)
  unset(Z_PREFIX_STR CACHE)
  set(ZLIB_ROOT "${CMAKE_BINARY_DIR}")
  if (NOT BRLCAD_COMPONENTS OR F_REQUIRED)
    find_package(ZLIB REQUIRED)
  else ()
    find_package(ZLIB)
  endif ()
  if ("${ZLIB_LIBRARIES}" MATCHES "${CMAKE_BINARY_DIR}/.*")
    set(Z_PREFIX_STR "brl_" CACHE STRING "Using local zlib" FORCE)
  endif ("${ZLIB_LIBRARIES}" MATCHES "${CMAKE_BINARY_DIR}/.*")

endmacro(find_package_zlib)

# Eigen - linear algebra library
macro(find_package_eigen)

  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(Eigen3 RESET_TP)
  set(Eigen3_ROOT "${BRLCAD_EXT_NOINSTALL_DIR}/share/eigen3/cmake")
  if (F_REQUIRED)
    find_package(Eigen3 NO_MODULE REQUIRED)
  else ()
    find_package(Eigen3 NO_MODULE)
  endif ()
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} Eigen)
  list(REMOVE_DUPLICATES SYS_INCLUDE_PATTERNS)
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} Eigen CACHE STRING "Bundled system include dirs" FORCE)

  # Let the cache know for BRLCAD_Summary.cmake
  set(EIGEN3_INCLUDE_DIR "${EIGEN3_INCLUDE_DIR}" CACHE PATH "Eigen include directory" FORCE)

endmacro(find_package_eigen)

# OpenCV - Open Source Computer Vision Library
# http://opencv.org
macro(find_package_opencv)

  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(OpenCV RESET_TP)

  # First, see if we have a bundled version
  set(OpenCV_DIR_TMP "${OpenCV_DIR}")
  set(OpenCV_DIR "${CMAKE_BINARY_DIR}/${LIB_DIR}/cmake/opencv4")
  set(OpenCV_ROOT ${CMAKE_BINARY_DIR})
  find_package(OpenCV COMPONENTS core features2d imgproc highgui)
  unset(OpenCV_ROOT)

  # If no bundled copy, see what the system has
  if (NOT OpenCV_FOUND)
    set(OpenCV_DIR "${OpenCV_DIR_TMP}")
    if (F_REQUIRED)
      find_package(OpenCV REQUIRED)
    else()
      find_package(OpenCV)
    endif ()
    if (OpenCV_FOUND)
      set(OPENCV_STATUS "System" CACHE STRING "OpenCV is bundled" FORCE)
    else (OpenCV_FOUND)
      set(OPENCV_STATUS "NotFound" CACHE STRING "OpenCV is bundled" FORCE)
    endif (OpenCV_FOUND)

  else (NOT OpenCV_FOUND)
    set(OPENCV_STATUS "Bundled" CACHE STRING "OpenCV is bundled" FORCE)
  endif (NOT OpenCV_FOUND)

endmacro(find_package_opencv)

# TCL - scripting language.  For Tcl/Tk builds we want
# static lib building on so we get the stub libraries.
macro(find_package_tcl)

  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  if (BRLCAD_ENABLE_TK)
    # For FindTCL.cmake
    set(TCL_ENABLE_TK ON CACHE BOOL "enable tk")
  endif (BRLCAD_ENABLE_TK)
  mark_as_advanced(TCL_ENABLE_TK)

  find_package_reset(TCL RESET_TP)
  find_package_reset(TK RESET_TP)
  if (RESET_TP)
    unset(TCL_INCLUDE_PATH CACHE)
    unset(TCL_STUB_LIBRARY CACHE)
    unset(TCL_TCLSH CACHE)
    unset(TK_INCLUDE_PATH CACHE)
    unset(TK_STUB_LIBRARY CACHE)
    unset(TK_WISH CACHE)
    unset(TK_X11_GRAPHICS CACHE)
    unset(TTK_STUB_LIBRARY CACHE)
  endif (RESET_TP)

  set(TCL_ROOT "${CMAKE_BINARY_DIR}")
  if (F_REQUIRED)
    find_package(TCL REQUIRED)
  else()
    find_package(TCL)
  endif ()

endmacro(find_package_tcl)


# Qt - cross-platform user interface/application development toolkit
# https://download.qt.io/archive/qt
macro(find_package_qt)

  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(Qt5 RESET_TP)
  find_package_reset(Qt6 RESET_TP)

  set(QtComponents Core Widgets Gui Svg Network)
  if (BRLCAD_ENABLE_OPENGL)
    set(QtComponents  ${QtComponents} OpenGL OpenGLWidgets)
  endif (BRLCAD_ENABLE_OPENGL)

  if (RESET_TP)
    foreach(qc ${QtComponents})
      unset(Qt6${qc}_DIR CACHE)
      unset(Qt6${qc}_FOUND CACHE)
      unset(Qt5${qc}_DIR CACHE)
      unset(Qt5${qc}_FOUND CACHE)
    endforeach(qc ${QtComponents})
  endif (RESET_TP)

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
    list(REMOVE_ITEM QtComponents OpenGLWidgets)
    if (F_REQUIRED)
      # This is our last attempt - if we're requiring Qt and this fails,
      # it's fatal
      find_package(Qt5 COMPONENTS ${QtComponents} REQUIRED)
    else()
      find_package(Qt5 COMPONENTS ${QtComponents})
    endif()
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
endmacro(find_package_qt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

