# Copyright (c) 2010-2025 United States Government as represented by
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

include(CMakeParseArguments)


# Preliminary setup and variable population
function(brlcad_bext_init BEXT_SHA1)

  # ******************************************************************
  # We store the targeted bext repository hash (BEXT_SHA1) in the top
  # level CMakeLists.txt file, passed as an arg.  The hash needs to be
  # updated whenever BRL-CAD needs to target a new version of bext.
  # ******************************************************************

  # Populate these early, even though their main use is in
  # misc/CMake/BRLCAD_ExternalDeps.cmake - find_program and
  # find_package calls may also make use of them, particularly
  # BRLCAD_EXT_NOINSTALL_DIR
  set(BRLCAD_EXT_DIR_ENV "$ENV{BRLCAD_EXT_DIR}")
  if(BRLCAD_EXT_DIR_ENV AND NOT DEFINED BRLCAD_EXT_DIR)
    set(BRLCAD_EXT_DIR ${BRLCAD_EXT_DIR_ENV})
  endif(BRLCAD_EXT_DIR_ENV AND NOT DEFINED BRLCAD_EXT_DIR)

  # Handle a pre-defined BRLCAD_EXT_DIR variable
  if(DEFINED BRLCAD_EXT_DIR)
    # Make sure to cache BRLCAD_EXT_DIR setting - otherwise, CMake
    # re-configure is going to ignore a previously specified directory
    set(BRLCAD_EXT_DIR "${BRLCAD_EXT_DIR}" CACHE PATH "BRL-CAD external dependency sources")

    if(NOT DEFINED BRLCAD_EXT_INSTALL_DIR AND EXISTS "${BRLCAD_EXT_DIR}/install")
      set(BRLCAD_EXT_INSTALL_DIR "${BRLCAD_EXT_DIR}/install")
    endif(NOT DEFINED BRLCAD_EXT_INSTALL_DIR AND EXISTS "${BRLCAD_EXT_DIR}/install")
    # Need to handle the case where BRLCAD_EXT_DIR is a symlink - if
    # it is, we need to expand the symlink in order for the tar tricks
    # we use later for file copying to work...
    if(DEFINED BRLCAD_EXT_INSTALL_DIR AND IS_SYMLINK ${BRLCAD_EXT_INSTALL_DIR})
      file(REAL_PATH "${BRLCAD_EXT_INSTALL_DIR}" EXT_PATH)
      set(BRLCAD_EXT_INSTALL_DIR "${EXT_PATH}")
    endif(DEFINED BRLCAD_EXT_INSTALL_DIR AND IS_SYMLINK ${BRLCAD_EXT_INSTALL_DIR})

    # For noinstall we don't need to worry about symlinks since we'll
    # be using the contents in place.
    if(NOT DEFINED BRLCAD_EXT_NOINSTALL_DIR AND EXISTS "${BRLCAD_EXT_DIR}/noinstall")
      set(BRLCAD_EXT_NOINSTALL_DIR "${BRLCAD_EXT_DIR}/noinstall")
    endif(NOT DEFINED BRLCAD_EXT_NOINSTALL_DIR AND EXISTS "${BRLCAD_EXT_DIR}/noinstall")
  endif(DEFINED BRLCAD_EXT_DIR)

  # We've got to have bext for at least a few custom components no
  # matter how many system packages are installed.
  if(NOT DEFINED BRLCAD_EXT_NOINSTALL_DIR OR NOT DEFINED BRLCAD_EXT_INSTALL_DIR)
    message(
      WARNING
      "External dependencies will be downloaded, configured, and built automatically as-needed. Set BRLCAD_EXT_DIR to specify instead."
    )
    message(
      STATUS
      "BRLCAD_EXT_DIR specifies prebuilt external dependencies directory\n"
      "containing 'install' and 'noinstall' folders, outputs from installing\n"
      "https://github.com/BRL-CAD/bext.\n"
    )
  endif(NOT DEFINED BRLCAD_EXT_NOINSTALL_DIR OR NOT DEFINED BRLCAD_EXT_INSTALL_DIR)

  # Before configure, check whether there's a type mismatch with bext
  # and BRL-CAD build types.  Some platforms (Windows), that's fatal.
  #
  # NOTE: This test must come AFTER inclusion of BRLCAD_Build_Types
  if(CMAKE_BUILD_TYPE AND EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}")
    find_program(DUMPBIN_EXEC dumpbin)
    set(TEST_BINFILE ${BRLCAD_EXT_NOINSTALL_DIR}/bin/strclear.exe)
    if(DUMPBIN_EXEC AND EXISTS ${TEST_BINFILE})
      # dumpbin doesn't like CMake style paths
      file(TO_NATIVE_PATH "${TEST_BINFILE}" TBFN)
      # https://stackoverflow.com/a/28304716/2037687
      execute_process(COMMAND ${DUMPBIN_EXEC} /dependents ${TBFN} OUTPUT_VARIABLE DB_OUT ERROR_VARIABLE DB_OUT)
      if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	if("${DB_OUT}" MATCHES ".*MSVCP[0-9]*d.dll.*" OR "${DB_OUT}" MATCHES ".*MSVCP[0-9]*D.dll.*")
	  message(
	    FATAL_ERROR
	    "Release build specified, but supplied bext binaries in ${BRLCAD_EXT_DIR} are compiled as Debug binaries."
	  )
	endif("${DB_OUT}" MATCHES ".*MSVCP[0-9]*d.dll.*" OR "${DB_OUT}" MATCHES ".*MSVCP[0-9]*D.dll.*")
      endif("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
      if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	if(NOT "${DB_OUT}" MATCHES ".*MSVCP[0-9]*d.dll.*" AND NOT "${DB_OUT}" MATCHES ".*MSVCP[0-9]*D.dll.*")
	  message(
	    FATAL_ERROR
	    "Debug build specified, but supplied bext binaries in ${BRLCAD_EXT_DIR} are compiled as Release binaries."
	  )
	endif(NOT "${DB_OUT}" MATCHES ".*MSVCP[0-9]*d.dll.*" AND NOT "${DB_OUT}" MATCHES ".*MSVCP[0-9]*D.dll.*")
      endif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    endif(DUMPBIN_EXEC AND EXISTS ${TEST_BINFILE})
  endif(CMAKE_BUILD_TYPE AND EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}")

  # Persist key variables
  set(BEXT_SHA1 ${BEXT_SHA1} PARENT_SCOPE)
  set(BRLCAD_EXT_DIR ${BRLCAD_EXT_DIR} PARENT_SCOPE)
  set(BRLCAD_EXT_INSTALL_DIR ${BRLCAD_EXT_INSTALL_DIR} PARENT_SCOPE)
  set(BRLCAD_EXT_NOINSTALL_DIR ${BRLCAD_EXT_NOINSTALL_DIR} PARENT_SCOPE)

endfunction()


# Build a local copy of bext if we were unable to locate one

function(brlcad_rel_version BRLCAD_REL)
  file(READ ${CMAKE_SOURCE_DIR}/include/conf/MAJOR VMAJOR)
  file(READ ${CMAKE_SOURCE_DIR}/include/conf/MINOR VMINOR)
  file(READ ${CMAKE_SOURCE_DIR}/include/conf/PATCH VPATCH)
  string(STRIP "${VMAJOR}" VMAJOR)
  string(STRIP "${VMINOR}" VMINOR)
  string(STRIP "${VPATCH}" VPATCH)
  set(${BRLCAD_REL} "rel-${VMAJOR}-${VMINOR}-${VPATCH}" PARENT_SCOPE)
endfunction()

# Look up branch or tag SHA1 hashes from primary github.com repo
function(remote_sha1 SHA1_VAR BRANCH)
  if(NOT GIT_EXEC)
    set(${SHA1_VAR} "" PARENT_SCOPE)
    return()
  endif()
  # This needs a working internet connection to succeed (and GitHub
  # must be up and working as well.)
  execute_process(
    COMMAND ${GIT_EXEC} ls-remote https://github.com/BRL-CAD/bext.git ${BRANCH}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE LS_REMOTE_STATUS
    OUTPUT_VARIABLE LS_REMOTE_STR
    ERROR_VARIABLE LS_REMOTE_STR
    )
  if(NOT LS_REMOTE_STATUS AND LS_REMOTE_STR)
    string(REPLACE " " ";" LS_REMOTE_STR "${LS_REMOTE_STR}")
    string(REPLACE "\t" ";" LS_REMOTE_STR "${LS_REMOTE_STR}")
    list(GET LS_REMOTE_STR 0 REMOTE_SHA1)
    set(${SHA1_VAR} "${REMOTE_SHA1}" PARENT_SCOPE)
  else()
    set(${SHA1_VAR} "" PARENT_SCOPE)
  endif()
endfunction()

# Do a variety of checks and validations on the SHA1 hashes of bext
function(bext_sha1_checks)

  # If we have a pre-defined source dir and it's not in BRL-CAD's
  # source tree, skip bext checks - just use what user has defined.
  if(DEFINED BRLCAD_EXT_SOURCE_DIR)
    IS_SUBPATH("${CMAKE_SOURCE_DIR}" "${BRLCAD_EXT_SOURCE_DIR}" BEXT_SUBDIR)
    if (NOT BEXT_SUBDIR)
      return()
    endif()
  endif()

  # If non-git archive, we don't have enough info to check bext.
  if (NOT EXISTS "${PROJECT_SOURCE_DIR}/.git")
    return()
  endif()
  # Ditto git executable - we need it for this
  find_program(GIT_EXEC git)
  if (NOT GIT_EXEC)
    return()
  endif()

  # Next two tags are used for validation of the currently specified
  # SHA1 against the upstream bext repository state
  brlcad_rel_version(BRLCAD_REL)
  remote_sha1(REMOTE_VER_SHA1 ${BRLCAD_REL})
  set(TARGET_BRANCH main)
  remote_sha1(REMOTE_BRANCH_SHA1 ${TARGET_BRANCH})

  # If re-configuring, we may need to redo an obsolete bext build
  set(OLD_SHA1)
  if (EXISTS "${CMAKE_CURRENT_BINARY_DIR}/bext.sha1")
    file(READ "${CMAKE_CURRENT_BINARY_DIR}/bext.sha1" OLD_SHA1)
  endif()

  # If this logic isn't working, first thing to check is whether
  # we are successfully getting SHA1 hashes
  #message("bext SHA1: ${BEXT_SHA1}")
  #message("remote VERSION SHA1: ${REMOTE_VER_SHA1}")
  #message("remote ${TARGET_BRANCH} SHA1: ${REMOTE_BRANCH_SHA1}")
  #message("OLD_SHA1: ${OLD_SHA1}")

  # If not on a tag, check whether the specified SHA1 and the bext
  # sha1 match.  It's not (always) a show stopper if they don't, but
  # general expectation is the bext build will be kept up-to-date.
  if(BEXT_SHA1 AND REMOTE_BRANCH_SHA1 AND NOT REMOTE_VER_SHA1)
    if(NOT "${BEXT_SHA1}" STREQUAL "${REMOTE_BRANCH_SHA1}")
      message(
	WARNING
	"Local checkout SHA1 (${BEXT_SHA1}) does not match latest bext '${TARGET_BRANCH}' branch SHA1 (${REMOTE_BRANCH_SHA1}).  Mistmatch typically indicates either upstream repo updated and you need to update your local clone manually, or the brlcad_bext_init() argument in top level CMakeLists.txt needs ${REMOTE_BRANCH_SHA1}."
	)
    endif()
  endif(BEXT_SHA1 AND REMOTE_BRANCH_SHA1 AND NOT REMOTE_VER_SHA1)

  # If we have a tag, check whether the locally specified SHA1
  # matches.  This check is primarily useful during release prep.
  if(BEXT_SHA1 AND REMOTE_VER_SHA1)
    if(NOT "${BEXT_SHA1}" STREQUAL "${REMOTE_VER_SHA1}")
      message(
	WARNING
	"Local SHA1 (${BEXT_SHA1}) does not match ${BRLCAD_REL} SHA1 (${REMOTE_VER_SHA1}).  If preparing a release, remember to update the argument to brlcad_bext_init in the top level CMakeLists.txt file to point to bext's ${BRLCAD_REL} tag."
	)
    endif()
  endif(BEXT_SHA1 AND REMOTE_VER_SHA1)

  # A clone into the build dir is transient - we don't keep it after
  # configure and successful bext build in order to save space.  We
  # still want to detect if bext we built no longer matches upstream,
  # though, so we cache the bext SHA1.  If it is cached, see if it
  # matches our target BEXT_SHA1.  If not, we need to clear old bext
  # build products from the build directory and restart fresh.
  if (OLD_SHA1 AND NOT "${OLD_SHA1}" STREQUAL "${BEXT_SHA1}")
    message("Previously compiled bext build products do not match current target - resetting.")
    if (EXISTS "${CMAKE_INSTALL_PREFIX}/include/brlcad/bu.h")
      message(WARNING "${CMAKE_INSTALL_PREFIX} appears to contain a BRL-CAD install.  We are resetting bext components in the build, but not clearing installed contents.  To avoid unexpected behavior, clearing ${CMAKE_INSTALL_PREFIX} is recommended.")
    endif()
    # Remove all old bext output files in build directory
    if (EXISTS "${TP_INVENTORY}")
      file(READ "${TP_INVENTORY}" TP_P)
      string(REPLACE "\n" ";" TP_PREVIOUS "${TP_P}")
      foreach(ef ${TP_PREVIOUS})
	message("Removing ${CMAKE_BINARY_DIR}/${ef}")
	file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
      endforeach()
      file(REMOVE "${TP_INVENTORY}")
      file(REMOVE "${TP_INVENTORY_BINARIES}")
    endif()
    IS_SUBPATH("${CMAKE_BINARY_DIR}" "${BRLCAD_EXT_INSTALL_DIR}" EXT_SUBDIR)
    if (EXT_SUBDIR)
      message("Removing ${BRLCAD_EXT_INSTALL_DIR}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E rm -rf "${BRLCAD_EXT_INSTALL_DIR}")
    endif()
    IS_SUBPATH("${CMAKE_BINARY_DIR}" "${BRLCAD_EXT_NOINSTALL_DIR}" EXT_SUBDIR)
    if (EXT_SUBDIR)
      message("Removing ${BRLCAD_EXT_NOINSTALL_DIR}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E rm -rf "${BRLCAD_EXT_NOINSTALL_DIR}")
    endif()
  endif()

endfunction()

# Do setup work, update BEXT_SRC_CLEANUP based on whether
# the build needs to clear out the source directory.
function(setup_bext_dir)

  # If we have a pre-defined bext source dir and it's not in the
  # source tree, we skip setup checks and just use what user defined.
  if(DEFINED BRLCAD_EXT_SOURCE_DIR)
    IS_SUBPATH("${CMAKE_SOURCE_DIR}" "${BRLCAD_EXT_SOURCE_DIR}" BEXT_SUBDIR)
    if (NOT BEXT_SUBDIR)
      return()
    endif()
  endif()

  # If we don't have a user-specified source directory, it's time to clone.
  if (NOT BRLCAD_EXT_SOURCE_DIR)

    # With pre-populated bext source dir we can build without git, but
    # if we don't have bext sources AND we don't have git, we're done.
    if (NOT GIT_EXEC)
      message(FATAL_ERROR "No bext sources specified, and git executable not found.")
    endif()
    # Warn if we can't get a target SHA1 hash, since there's a good chance
    # we'll wind up with a bext that doesn't match the BRL-CAD checkout.
    if (NOT BEXT_SHA1)
      message(FATAL_ERROR "BEXT_SHA1 hash is unset.  This should be set in the top level BRL-CAD CMakeLists.txt file.")
    endif()

    # no pre-existing sources, so we're cloning into the build dir
    set(BRLCAD_EXT_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/bext")

    # Write current SHA1 for subsequent processing
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/bext.sha1" "${BEXT_SHA1}")
    distclean("${CMAKE_CURRENT_BINARY_DIR}/bext.sha1")

    # Clone
    message("BRL-CAD bext clone command: ${GIT_EXEC} clone https://github.com/BRL-CAD/bext.git")
    execute_process(
      COMMAND ${GIT_EXEC} clone https://github.com/BRL-CAD/bext.git
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    message("BRL-CAD bext checkout command: ${GIT_EXEC} -c advice.detachedHead=false checkout ${BEXT_SHA1}")
    execute_process(
      COMMAND ${GIT_EXEC} -c advice.detachedHead=false checkout ${BEXT_SHA1}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bext
    )

    # Configure process will need to clean up bext
    if (BRLCAD_BEXT_CLEANUP)
      set(BEXT_SRC_CLEANUP TRUE PARENT_SCOPE)
    endif (BRLCAD_BEXT_CLEANUP)

  endif()

  # Tell parent context the final bext source location
  set(BRLCAD_EXT_SOURCE_DIR "${BRLCAD_EXT_SOURCE_DIR}" PARENT_SCOPE)

endfunction()

function(brlcad_ext_setup)

  if(BRLCAD_DISABLE_RELOCATION)
    return()
  endif(BRLCAD_DISABLE_RELOCATION)

  find_program(GIT_EXEC git)

  set(BRLCAD_EXT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/bext_build)
  set(BRLCAD_EXT_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/bext_output)

  # If we don't have a bext source dir and BRLCAD_EXT_SOURCE_DIR isn't
  # set, try to clone.  setup_bext_dir() will set BEXT_SRC_CLEANUP if
  # we're out of date.
  set(BEXT_SRC_CLEANUP FALSE)
  setup_bext_dir()

  # If we don't have a valid bext one way or another by this point, we're done.
  if(NOT EXISTS ${BRLCAD_EXT_SOURCE_DIR})
    message(FATAL_ERROR "bext directory ${BRLCAD_EXT_SOURCE_DIR} is not present")
  endif(NOT EXISTS ${BRLCAD_EXT_SOURCE_DIR})
  if(NOT EXISTS "${BRLCAD_EXT_SOURCE_DIR}/dependencies.dot")
    message(FATAL_ERROR "Invalid bext directory: ${BRLCAD_EXT_SOURCE_DIR}")
  endif(NOT EXISTS "${BRLCAD_EXT_SOURCE_DIR}/dependencies.dot")

  # We do allow the user to specify a build dir to be reused, however
  # this is unpredictable and may cause unexpected results depending
  # on what is or isn't rebuilt - use with care.
  set(BEXT_BLD_CLEANUP FALSE)
  if(NOT EXISTS ${BRLCAD_EXT_BUILD_DIR})
    file(MAKE_DIRECTORY ${BRLCAD_EXT_BUILD_DIR})
    if (BRLCAD_BEXT_CLEANUP)
      # If the cleanup step is specifically enabled, we need to clear
      # the build dir after completing to save space.
      set(BEXT_BLD_CLEANUP TRUE)
    endif(BRLCAD_BEXT_CLEANUP)
  endif(NOT EXISTS ${BRLCAD_EXT_BUILD_DIR})

  # Need to control options for this based on BRL-CAD configure
  # settings.  Unlike an independent bext build, we know for this one
  # what we can turn on and off
  set(BEXT_ENABLE_ALL OFF)
  if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR ENABLE_ALL)
    set(BEXT_ENABLE_ALL ON)
  endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR ENABLE_ALL)
  set(BEXT_USE_GDAL ON)
  if(NOT BRLCAD_ENABLE_GDAL)
    set(BEXT_USE_GDAL OFF)
  endif(NOT BRLCAD_ENABLE_GDAL)
  set(BEXT_USE_QT ON)
  if(NOT BRLCAD_ENABLE_QT)
    set(BEXT_USE_QT OFF)
  endif(NOT BRLCAD_ENABLE_QT)
  set(BEXT_USE_TCL ON)
  if(NOT BRLCAD_ENABLE_TCL)
    set(BEXT_USE_TCL OFF)
  endif(NOT BRLCAD_ENABLE_TCL)
  set(BEXT_USE_APPLESEED OFF)
  if(BRLCAD_ENABLE_APPLESEED)
    set(BEXT_USE_APPLESEED ON)
  endif(BRLCAD_ENABLE_APPLESEED)

  # Define CMake arguments we'll need regardless of mode
  set(CMAKE_CMD_ARGS
    -S ${BRLCAD_EXT_SOURCE_DIR} -B ${BRLCAD_EXT_BUILD_DIR} -DGIT_SHALLOW_CLONE=ON
    -DENABLE_ALL=${BEXT_ENABLE_ALL} -DUSE_GDAL=${BEXT_USE_GDAL} -DUSE_QT=${BEXT_USE_QT} -DUSE_TCL=${BEXT_USE_TCL}
    -DUSE_APPLESEED=${BEXT_USE_APPLESEED} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${BRLCAD_EXT_INSTALL_DIR}
    )

  # If we're doing a components build, add the component list to the
  # CMake arguments
  if(BRLCAD_COMPONENTS)
    # To avoid problems involved with quoting semicolons on command
    # lines, we support using ':' as a delimiter in addition to the
    # standard CMake list entry delimiter ';'
    string(REPLACE ":" ";" BRLCAD_COMPONENTS "${BRLCAD_COMPONENTS}")

    set(active_dirs ${BRLCAD_COMPONENTS})
    foreach(wc ${BRLCAD_COMPONENTS})
      deps_expand(${wc} active_dirs)
    endforeach(wc ${BRLCAD_COMPONENTS})
    string(REPLACE ";" ":" active_dirs "${active_dirs}")
    list(APPEND CMAKE_CMD_ARGS -DBRLCAD_COMPONENTS=${active_dirs})
  endif(BRLCAD_COMPONENTS)

  # Let the user know what's happening
  message("${CMAKE_COMMAND} ${CMAKE_CMD_ARGS}")

  # Run bext configure
  execute_process(
    COMMAND ${CMAKE_COMMAND} ${CMAKE_CMD_ARGS}
    WORKING_DIRECTORY ${BRLCAD_EXT_BUILD_DIR}
    RESULT_VARIABLE EXT_CONFIG_STATUS
    )
  if(EXT_CONFIG_STATUS)
    message(FATAL_ERROR "Unable to successfully configure bext dependency repository for building")
  endif(EXT_CONFIG_STATUS)

  # define how many threads to compile with
  if(NOT DEFINED BRLCAD_EXT_PARALLEL)
    set(BRLCAD_EXT_PARALLEL 8)
  endif(NOT DEFINED BRLCAD_EXT_PARALLEL)

  message(
    "${CMAKE_COMMAND} --build ${BRLCAD_EXT_BUILD_DIR} --parallel ${BRLCAD_EXT_PARALLEL} --config ${CMAKE_BUILD_TYPE}"
    )

  execute_process(
    COMMAND
    ${CMAKE_COMMAND} --build ${BRLCAD_EXT_BUILD_DIR} --parallel ${BRLCAD_EXT_PARALLEL} --config ${CMAKE_BUILD_TYPE}
    RESULT_VARIABLE EXT_BUILD_STATUS
    )
  if(EXT_BUILD_STATUS)
    message(FATAL_ERROR "Unable to successfully build bext dependency repository")
  endif(EXT_BUILD_STATUS)

  # If cmake was successful and we set up bext src and build dirs,
  # clear them out to save space.  Highly beneficial for some
  # environments, e.g., Github runners,
  if(BEXT_SRC_CLEANUP)
    message("Removing ${BRLCAD_EXT_SOURCE_DIR}")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E rm -rf ${BRLCAD_EXT_SOURCE_DIR}
    )
  endif(BEXT_SRC_CLEANUP)
  if(BEXT_BLD_CLEANUP)
    message("Removing ${BRLCAD_EXT_BUILD_DIR}")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E rm -rf ${BRLCAD_EXT_BUILD_DIR}
    )
  endif(BEXT_BLD_CLEANUP)

  # Persist key variables in the cache
  set(BRLCAD_EXT_DIR ${BRLCAD_EXT_INSTALL_DIR} CACHE PATH "Local bext install" FORCE)
  set(BRLCAD_EXT_INSTALL_DIR ${BRLCAD_EXT_DIR}/install CACHE PATH "Local bext install" FORCE)
  set(BRLCAD_EXT_NOINSTALL_DIR ${BRLCAD_EXT_DIR}/noinstall CACHE PATH "Local bext install" FORCE)
endfunction(brlcad_ext_setup)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
