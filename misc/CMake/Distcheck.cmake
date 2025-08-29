#                 D I S T C H E C K . C M A K E
# BRL-CAD
#
# Copyright (c) 2012-2025 United States Government as represented by
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
# Distribution checking is the process of verifying that a source repository
# is in a valid, working state.  This file defines the top level macros that
# set up and control the process.  The distcheck targets need
# to have values substituted prior to inclusion in order to produce a command
# that will execute in parallel when using Make, so the distcheck target template
# is set up as a cmake.in file - configure_file and include are used to generate
# and source the final target definition for each case.

if("${CMAKE_VERBOSE_DISTCHECK}" STREQUAL "")
  set(CMAKE_VERBOSE_DISTCHECK OFF)
endif("${CMAKE_VERBOSE_DISTCHECK}" STREQUAL "")

find_program(CPACK_EXEC cpack)
mark_as_advanced(CPACK_EXEC)

# We'll always want the repo and source distcheck targets defined - they are constants
# for distcheck regardless of the build configurations used.

# Set up the script that will be used to verify the source archives
configure_file(
  "${BRLCAD_CMAKE_DIR}/distcheck_repo_verify.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_repo_verify.cmake"
  @ONLY
)
distclean("${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_repo_verify.cmake")

# Define the repository verification build target
add_custom_target(
  distcheck-repo_verify
  COMMAND ${CMAKE_COMMAND} -E echo "*** Check files in Source Repository against files specified in Build Logic ***"
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_repo_verify.cmake"
)
set_target_properties(distcheck-repo_verify PROPERTIES FOLDER "BRL-CAD Distribution Checking")

# When doing a raw package_source build, we have no choice but to copy the sources over
# right before we make the archive, since the CPack command has no awareness of the status
# of the source files.  When we're doing distcheck, on the other hand, we CAN be intelligent
# about triggering the copy rule - let's only do it once, since it's a rather slow operation.
add_custom_target(
  distcheck-source_archive_dir
  COMMAND ${CMAKE_COMMAND} -E echo "*** Prepare directory with archive contents in build directory ***"
  COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.done"
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake"
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.done"
  DEPENDS distcheck-repo_verify
)
set_target_properties(distcheck-source_archive_dir PROPERTIES FOLDER "BRL-CAD Distribution Checking")

# Define the source archive creation target - should only happen once repository is verified and
# source archive is set up.  Once archives are done, remove flag from distcheck-source_archive_dir
# to make sure a repeated distcheck works correctly.
add_custom_target(
  distcheck-source_archives
  COMMAND ${CMAKE_COMMAND} -E echo "*** Create source tgz, tbz2 and zip archives from toplevel archive ***"
  COMMAND ${CPACK_EXEC} --config "${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/source_archive_contents"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/_CPack_Packages"
  COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.done"
  DEPENDS distcheck-source_archive_dir
)
set_target_properties(distcheck-source_archives PROPERTIES FOLDER "BRL-CAD Distribution Checking")

# Utility function for defining individual distcheck targets
macro(
  create_distcheck
  TARGET_SUFFIX
  CMAKE_OPTS_IN
  source_dir
  build_dir
  install_dir
)
  # Check if a custom template was specified (optional)
  if(NOT "${ARGV5}" STREQUAL "")
    set(distcheck_template_file "${BRLCAD_CMAKE_DIR}/${ARGV5}")
  else(NOT "${ARGV5}" STREQUAL "")
    set(distcheck_template_file "${BRLCAD_CMAKE_DIR}/distcheck_target.cmake.in")
  endif(NOT "${ARGV5}" STREQUAL "")

  # If we've already got a particular distcheck target, don't try to create it again.
  if(NOT TARGET distcheck-${TARGET_SUFFIX})
    # Need to set these locally so configure_file will pick them up...
    set(TARGET_SUFFIX ${TARGET_SUFFIX})
    set(CMAKE_OPTS ${CMAKE_OPTS_IN})

    # For configure_file, need to set these as variables not just input parameters
    set(source_dir "${source_dir}")
    set(build_dir "${build_dir}")
    set(install_dir "${install_dir}")

    # Determine how to trigger the build in the distcheck target
    if("${CMAKE_GENERATOR}" MATCHES "Make")
      if(NOT CMAKE_VERBOSE_DISTCHECK)
        set(TARGET_REDIRECT " >> distcheck-${TARGET_SUFFIX}.log 2>&1")
        distclean("${CMAKE_CURRENT_BINARY_DIR}/distcheck-${TARGET_SUFFIX}.log")
      endif(NOT CMAKE_VERBOSE_DISTCHECK)
      set(DISTCHECK_BUILD_CMD "$(MAKE)")
      set(DISTCHECK_INSTALL_CMD "$(MAKE) install")
      set(DISTCHECK_REGRESS_CMD "$(MAKE) regress")
      set(DISTCHECK_TEST_CMD "$(MAKE) test")
    elseif("${CMAKE_GENERATOR}" MATCHES "Ninja")
      set(CMAKE_OPTS "-G Ninja ${CMAKE_OPTS}")
      if(NOT CMAKE_VERBOSE_DISTCHECK)
        set(TARGET_REDIRECT " >> distcheck-${TARGET_SUFFIX}.log 2>&1")
        distclean("${CMAKE_CURRENT_BINARY_DIR}/distcheck-${TARGET_SUFFIX}.log")
      endif(NOT CMAKE_VERBOSE_DISTCHECK)
      set(DISTCHECK_BUILD_CMD "ninja")
      set(DISTCHECK_INSTALL_CMD "ninja install")
      set(DISTCHECK_REGRESS_CMD "ninja regress")
      set(DISTCHECK_TEST_CMD "ninja test")
    else("${CMAKE_GENERATOR}" MATCHES "Make")
      set(DISTCHECK_BUILD_CMD "\"${CMAKE_COMMAND}\" --build .")
      set(DISTCHECK_INSTALL_CMD "\"${CMAKE_COMMAND}\" --build . --target install")
      set(DISTCHECK_REGRESS_CMD "\"${CMAKE_COMMAND}\" --build . --target regress")
      set(DISTCHECK_TEST_CMD "\"${CMAKE_COMMAND}\" --build . --target test")
      set(TARGET_REDIRECT "")
    endif("${CMAKE_GENERATOR}" MATCHES "Make")

    # Based on the build command, generate a distcheck target definition from the template
    configure_file(
      ${distcheck_template_file}
      "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target_${TARGET_SUFFIX}.cmake"
      @ONLY
    )
    distclean("${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target_${TARGET_SUFFIX}.cmake")
    include("${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target_${TARGET_SUFFIX}.cmake")

    # Keep track of the distcheck targets
    set(distcheck_targets ${distcheck_targets} distcheck-${TARGET_SUFFIX})
  else(NOT TARGET distcheck-${TARGET_SUFFIX})
    message(WARNING "Distcheck target distcheck-${TARGET_SUFFIX} already defined, skipping...")
  endif(NOT TARGET distcheck-${TARGET_SUFFIX})

endmacro(create_distcheck)

# Most distcheck targets deliberately specify a build type.  Since we are
# reusing the bext outputs produced by the parent configure (and we *want* to
# do that whenever possible - its a massive overall savings in build time) we
# can't define these for Windows due to the incompatibility of Debug and
# Release dlls.
# At the moment this isn't a practical issue, since distcheck is not known to
# work on Windows currently in any case, but eventually we'll have to make some
# adjustments.  Unfortunately, the most practical answer will probably be to
# have an option to force the individual configurations to each build their own
# copies of bext when we can't mix Debug and Release libs...

create_distcheck(default_build_type "" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")

if(NOT HAVE_WINDOWS_H)
  # Most of the tests will use the bext supplied to the original parent BRL-CAD configure, but we also
  # want to verify that the "configure driven" bext build works as well.
  create_distcheck(debug   "-DCMAKE_BUILD_TYPE=Debug" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(release "-DCMAKE_BUILD_TYPE=Release" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(enableall_debug   "-DCMAKE_BUILD_TYPE=Debug -DENABLE_ALL=ON" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(enableall_release "-DCMAKE_BUILD_TYPE=Release -DENABLE_ALL=ON" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(prepared_debug   "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR}" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(prepared_release "-DCMAKE_BUILD_TYPE=Release -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR}" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(no_tcl "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR} -DBRLCAD_ENABLE_TCL=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install" distcheck_no_tcl.cmake.in)
  create_distcheck(no_tk "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR} -DBRLCAD_ENABLE_TK=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(no_strict "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR} -DBRLCAD_ENABLE_STRICT=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(no_object "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR} -DUSE_OBJECT_LIBS=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  create_distcheck(odd_pathnames "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR}" "1 Odd_ source dir ++" "1 Odd_ build dir ++" "1 Odd_ install dir ++")
  create_distcheck(in_src_dir "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_EXT_DIR=${BRLCAD_EXT_DIR}" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "install" distcheck_in_src_dir.cmake.in)
endif(NOT HAVE_WINDOWS_H)

# Now that we're set up and have added the extra targets we want for distcheck-full, define the build targets
add_custom_target(
  distcheck-full
  # The source repository verification script is responsible for generating these files
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_remove_archives_if_invalid"
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_message"
  DEPENDS ${distcheck_targets}
)
set_target_properties(distcheck-full PROPERTIES FOLDER "BRL-CAD Distribution Checking")
set_target_properties(distcheck-full PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

add_custom_target(distcheck DEPENDS distcheck-full)
set_target_properties(distcheck PROPERTIES FOLDER "BRL-CAD Distribution Checking")

# Write out and compare the distcheck.yml actions file.
set(distcheck_yml_out "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck.yml")
distclean("${distcheck_yml_out}")

# The distcheck targets don't get defined (at the moment) on all platforms.
# Consequently, we can't update distcheck.yml on all platforms. Someday
# we should get these all working on Windows, but for the moment just do
# here what we do for the more exotic build types - we want distcheck.yml
# to be comprehensive, so the truncated Windows set won't do.
if(NOT HAVE_WINDOWS_H)
  function(emit_job JOBNAME PREVJOB)
    file(APPEND ${distcheck_yml_out} "  ${JOBNAME}:\n")
    file(APPEND ${distcheck_yml_out} "    name: ${JOBNAME}\n")
    file(APPEND ${distcheck_yml_out} "    runs-on: ubuntu-latest\n")
    if(NOT \"${PREVJOB}\" STREQUAL \"\")
      file(APPEND ${distcheck_yml_out} "    needs: [${PREVJOB}]\n")
    endif()
    file(APPEND ${distcheck_yml_out} "    env:\n")
    file(APPEND ${distcheck_yml_out} "      DEBIAN_FRONTEND: noninteractive\n")
    file(APPEND ${distcheck_yml_out} "    if: github.ref == 'refs/heads/main'\n")
    file(APPEND ${distcheck_yml_out} "    steps:\n")
    file(APPEND ${distcheck_yml_out} "      - name: Setup - CMake\n")
    file(APPEND ${distcheck_yml_out} "        uses: lukka/get-cmake@latest\n")
    file(APPEND ${distcheck_yml_out} "      - name: Cache apt packages\n")
    file(APPEND ${distcheck_yml_out} "        uses: actions/cache@v4\n")
    file(APPEND ${distcheck_yml_out} "        with:\n")
    file(APPEND ${distcheck_yml_out} "          path: |\n")
    file(APPEND ${distcheck_yml_out} "            /var/cache/apt/archives\n")
    file(APPEND ${distcheck_yml_out} "          key: \${{ runner.os }}-apt-\${{ hashFiles('.github/workflows/check.yml') }}\n")
    file(APPEND ${distcheck_yml_out} "      - name: Install apt dependencies\n")
    file(APPEND ${distcheck_yml_out} "        run: |\n")
    file(APPEND ${distcheck_yml_out} "          sudo apt-get update\n")
    file(APPEND ${distcheck_yml_out} "          sudo apt-get install xserver-xorg-dev libx11-dev libxi-dev libxext-dev libglu1-mesa-dev libfontconfig-dev\n")
    file(APPEND ${distcheck_yml_out} "          sudo apt-get install astyle re2c xsltproc libxml2-utils\n")
    if ("${JOBNAME}" STREQUAL "distcheck-no_tk")
      # For the no_tk case we deliberately avoid installing any system Tcl/Tk to
      # help avoid things accidentally working
      file(APPEND ${distcheck_yml_out} "          sudo apt-get install zlib1g-dev libpng-dev libjpeg-dev libtiff-dev libeigen3-dev libgdal-dev libassimp-dev libopencv-dev libgl-dev libinput-dev\n\n")
    else ()
      file(APPEND ${distcheck_yml_out} "          sudo apt-get install zlib1g-dev libpng-dev libjpeg-dev libtiff-dev libeigen3-dev libgdal-dev libassimp-dev libopencv-dev tcl-dev tk-dev libgl-dev libinput-dev\n\n")
    endif()
    file(APPEND ${distcheck_yml_out} "      - name: Setup - bext\n")
    file(APPEND ${distcheck_yml_out} "        run: |\n")
    file(APPEND ${distcheck_yml_out} "          git clone https://github.com/BRL-CAD/bext.git\n")
    file(APPEND ${distcheck_yml_out} "          cd bext\n")
    file(APPEND ${distcheck_yml_out} "          echo \"sha=$(git rev-parse HEAD)\" >> $GITHUB_OUTPUT\n")
    file(APPEND ${distcheck_yml_out} "          cd ..\n\n")
    file(APPEND ${distcheck_yml_out} "      - name: Cache bext build outputs\n")
    file(APPEND ${distcheck_yml_out} "        id: cache-bext\n")
    file(APPEND ${distcheck_yml_out} "        uses: actions/cache@v4\n")
    file(APPEND ${distcheck_yml_out} "        with:\n")
    file(APPEND ${distcheck_yml_out} "          path: \${{ github.workspace }}/bext_output\n")
    file(APPEND ${distcheck_yml_out} "          key: \${{ runner.os }}-bext-\${{ steps.bext-sha.outputs.sha }}\n")
    file(APPEND ${distcheck_yml_out} "      - name: Checkout\n")
    file(APPEND ${distcheck_yml_out} "        uses: actions/checkout@v4\n")
    file(APPEND ${distcheck_yml_out} "        with:\n")
    file(APPEND ${distcheck_yml_out} "          path: brlcad\n")
    file(APPEND ${distcheck_yml_out} "      - name: Directory setup\n")
    file(APPEND ${distcheck_yml_out} "        run: cmake -E make_directory build_${JOBNAME}\n")
    file(APPEND ${distcheck_yml_out} "      - name: Configure\n")
    file(APPEND ${distcheck_yml_out} "        run: cmake -S brlcad -B build_${JOBNAME} -G Ninja -DCMAKE_BUILD_TYPE=Release -DBRLCAD_EXT_DIR=\${{ github.workspace }}/bext_output\n")
    file(APPEND ${distcheck_yml_out} "      - name: Build\n")
    file(APPEND ${distcheck_yml_out} "        run: cmake --build build_${JOBNAME} --config Release --target ${JOBNAME}\n")
    file(APPEND ${distcheck_yml_out} "      - name: Log\n")
    file(APPEND ${distcheck_yml_out} "        if: always()\n")
    file(APPEND ${distcheck_yml_out} "        run: cat build_${JOBNAME}/${JOBNAME}.log\n")
    file(APPEND ${distcheck_yml_out} "      - name: Debug if failure\n")
    file(APPEND ${distcheck_yml_out} "        if: failure()\n")
    file(APPEND ${distcheck_yml_out} "        uses: mxschmitt/action-tmate@v3\n\n")
  endfunction()

  execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/misc/CMake/distcheck_hdr.yml" "${distcheck_yml_out}")
  set(PREV_JOB "bext")
  foreach(JOB ${distcheck_targets})
    emit_job("${JOB}" "${PREV_JOB}")
    set(PREV_JOB "${JOB}")
  endforeach()

  # Now compare to .github/workflows/distcheck.yml
  file(READ "${distcheck_yml_out}" NEW_YML)
  string(REPLACE "\r\n" "\n" NEW_YML "${NEW_YML}")
  file(READ "${PROJECT_SOURCE_DIR}/.github/workflows/distcheck.yml" EXISTING_YML)
  string(REPLACE "\r\n" "\n" EXISTING_YML "${EXISTING_YML}")

  if(NOT "${NEW_YML}" STREQUAL "${EXISTING_YML}")
    message(FATAL_ERROR
      "distcheck.yml is out of sync with .github/workflows/distcheck.yml.\n"
      "Please copy ${distcheck_yml_out} to .github/workflows/distcheck.yml"
    )
  endif()
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
