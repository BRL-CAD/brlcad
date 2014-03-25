#                 D I S T C H E C K . C M A K E
# BRL-CAD
#
# Copyright (c) 2012-2014 United States Government as represented by
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

if(NOT BRLCAD_IS_SUBBUILD)
  if("${CMAKE_VERBOSE_DISTCHECK}" STREQUAL "")
    set(CMAKE_VERBOSE_DISTCHECK OFF)
  endif("${CMAKE_VERBOSE_DISTCHECK}" STREQUAL "")

  find_program(CPACK_EXEC cpack)
  mark_as_advanced(CPACK_EXEC)

  # We'll always want the repo and source distcheck targets defined - they are constants
  # for distcheck regardless of the build configurations used.

  # Set up the script that will be used to verify the source archives
  configure_file(${BRLCAD_CMAKE_DIR}/distcheck_repo_verify.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_repo_verify.cmake @ONLY)

  # Define the repository verification build target
  add_custom_target(distcheck-repo_verify
    COMMAND ${CMAKE_COMMAND} -E echo "*** Check files in Source Repository against files specified in Build Logic ***"
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_repo_verify.cmake)


  # When doing a raw package_source build, we have no choice but to copy the sources over
  # right before we make the archive, since the CPack command has no awareness of the status
  # of the source files.  When we're doing distcheck, on the other hand, we CAN be intelligent
  # about triggering the copy rule - let's only do it once, since it's a rather slow operation.
  set(create_source_archive_dir_script "
  CMAKE_POLICY(SET CMP0007 OLD)
  file(REMOVE_RECURSE \"${CMAKE_BINARY_DIR}/source_archive_contents\")
  file(STRINGS ${CMAKE_BINARY_DIR}/cmakefiles.cmake source_tree_files)
  file(STRINGS ${CMAKE_BINARY_DIR}/cmakedirs.cmake ignored_dirctories)
  string(REPLACE \"${BRLCAD_SOURCE_DIR}\" \"------BRLCAD_SOURCE_DIR----\" ignored_directories \"${ignored_directories}\")
  foreach(ITEM \${ignored_dirctories})
    file(GLOB_RECURSE dir_files \"\${ITEM}/*\")
    list(APPEND source_tree_files \${dir_files})
    while(dir_files)
      set(dir_files \"\${dir_files};\")
      string(REGEX REPLACE \"[^/]*;\" \";\" dir_files \"\${dir_files}\")
      string(REGEX REPLACE \"/;\" \";\" dir_files \"\${dir_files}\")
      list(REMOVE_DUPLICATES dir_files)
      list(APPEND source_tree_files \${dir_files})
    endwhile(dir_files)
  endforeach(ITEM \${ignored_dirctories})
  string(REPLACE \"------BRLCAD_SOURCE_DIR----\" \"${BRLCAD_SOURCE_DIR}\" source_tree_files \"\${source_tree_files}\")
  foreach(source_file \${source_tree_files})
    if(NOT IS_DIRECTORY \${source_file})
      string(REPLACE \"${CMAKE_SOURCE_DIR}/\" \"\" relative_name \"\${source_file}\")
      if(NOT \"\${relative_name}\" MATCHES \".svn/\")
   string(REPLACE \"${CMAKE_SOURCE_DIR}/\" \"${CMAKE_BINARY_DIR}/source_archive_contents/\" outfile \"\${source_file}\")
   execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy \${source_file} \${outfile})
      endif(NOT \"\${relative_name}\" MATCHES \".svn/\")
    endif(NOT IS_DIRECTORY \${source_file})
  endforeach(source_file \${source_tree_files})
  ")
  file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.cmake "${create_source_archive_dir_script}")
  add_custom_target(distcheck-source_archive_dir
    COMMAND ${CMAKE_COMMAND} -E echo "*** Prepare directory with archive contents in build directory ***"
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.done
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.done
    DEPENDS distcheck-repo_verify)

  # Define the source archive creation target - should only happen once repository is verified and
  # source archive is set up.  Once archives are done, remove flag from distcheck-source_archive_dir
  # to make sure a repeated distcheck works correctly.
  add_custom_target(distcheck-source_archives
    COMMAND ${CMAKE_COMMAND} -E echo "*** Create source tgz, tbz2 and zip archives from toplevel archive ***"
    COMMAND ${CPACK_EXEC} --config ${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeTmp/create_builddir_source_archive.done
    DEPENDS distcheck-source_archive_dir)

  # Utility function for defining individual distcheck targets
  macro(CREATE_DISTCHECK TARGET_SUFFIX CMAKE_OPTS source_dir build_dir install_dir)
    # Check if a custom template was specified (optional)
    if(NOT "${ARGV5}" STREQUAL "")
      set(distcheck_template_file "${BRLCAD_CMAKE_DIR}/${ARGV5}")
    else(NOT "${ARGV5}" STREQUAL "")
      set(distcheck_template_file "${BRLCAD_CMAKE_DIR}/distcheck_target.cmake.in")
    endif(NOT "${ARGV5}" STREQUAL "")

    # If we've already got a particular distcheck target, don't try to create it again.
    if(NOT TARGET distcheck-${TARGET_SUFFIX})
      # Need to set these locally so configure_file will pick them up...
      SET(TARGET_SUFFIX ${TARGET_SUFFIX})
      SET(CMAKE_OPTS ${CMAKE_OPTS})

      # For configure_file, need to set these as variables not just input parameters
      set(source_dir "${source_dir}")
      set(build_dir "${build_dir}")
      set(install_dir "${install_dir}")

      # Determine how to trigger the build in the distcheck target
      if("${CMAKE_GENERATOR}" MATCHES "Make")
	if(NOT CMAKE_VERBOSE_DISTCHECK)
	  set(TARGET_REDIRECT " >> distcheck-${TARGET_SUFFIX}.log 2>&1")
	  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/distcheck-${TARGET_SUFFIX}.log)
	endif(NOT CMAKE_VERBOSE_DISTCHECK)
	set(DISTCHECK_BUILD_CMD "$(MAKE)")
	set(DISTCHECK_INSTALL_CMD "$(MAKE) install")
	set(DISTCHECK_REGRESS_CMD "$(MAKE) regress")
	set(DISTCHECK_TEST_CMD "$(MAKE) test")
      else("${CMAKE_GENERATOR}" MATCHES "Make")
	set(DISTCHECK_BUILD_CMD "\"${CMAKE_COMMAND}\" -E build .")
	set(DISTCHECK_INSTALL_CMD "\"${CMAKE_COMMAND}\" -E build . --target install")
	set(DISTCHECK_REGRESS_CMD "\"${CMAKE_COMMAND}\" -E build . --target regress")
	set(DISTCHECK_TEST_CMD "\"${CMAKE_COMMAND}\" -E build . --target test")
	set(TARGET_REDIRECT "")
      endif("${CMAKE_GENERATOR}" MATCHES "Make")

      # Based on the build command, generate a distcheck target definition from the template
      configure_file(${distcheck_template_file} ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target_${TARGET_SUFFIX}.cmake @ONLY)
      include(${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target_${TARGET_SUFFIX}.cmake)

      # Keep track of the distcheck targets
      set(distcheck_targets ${distcheck_targets} distcheck-${TARGET_SUFFIX})
    else(NOT TARGET distcheck-${TARGET_SUFFIX})
      message(WARNING "Distcheck target distcheck-${TARGET_SUFFIX} already defined, skipping...")
    endif(NOT TARGET distcheck-${TARGET_SUFFIX})
  endmacro(CREATE_DISTCHECK)

  # Top level macro for defining the common "standard" cases and lets a CMake build select either
  # the standard Debug + Release or all defined distchecks as the build that is triggered by the
  # distcheck target - pass either STD or FULL as an argument to select one or the other.  Default
  # if not argument is supplied is STD.
  macro(DEFINE_DISTCHECK_TARGET)
    if(${ARGC} EQUAL 1)
      string(TOUPPER ${ARGV0} ARGV0_UPPER)
      if(${ARGV0_UPPER} STREQUAL "STD" OR ${ARGV0_UPPER} STREQUAL "STANDARD")
	set(full_distcheck 0)
      endif(${ARGV0_UPPER} STREQUAL "STD" OR ${ARGV0_UPPER} STREQUAL "STANDARD")
      if(${ARGV0_UPPER} STREQUAL "FULL")
	set(full_distcheck 1)
      endif(${ARGV0_UPPER} STREQUAL "FULL")
    endif(${ARGC} EQUAL 1)

    CREATE_DISTCHECK(enableall_debug   "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
    CREATE_DISTCHECK(enableall_release "-DCMAKE_BUILD_TYPE=Release -DBRLCAD_BUNDLED_LIBS=BUNDLED" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")

    add_custom_target(distcheck-std
      # The source repository verification script is responsible for generating these files
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_remove_archives_if_invalid
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_message
      DEPENDS distcheck-enableall_debug distcheck-enableall_release)

    add_custom_target(distcheck-full
      # The source repository verification script is responsible for generating these files
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_remove_archives_if_invalid
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_message
      DEPENDS ${distcheck_targets})

    if(full_distcheck)
      add_custom_target(distcheck DEPENDS distcheck-full)
    else(full_distcheck)
      add_custom_target(distcheck DEPENDS distcheck-std)
    endif(full_distcheck)
  endmacro(DEFINE_DISTCHECK_TARGET)
endif(NOT BRLCAD_IS_SUBBUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
