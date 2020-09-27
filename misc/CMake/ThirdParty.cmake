#                T H I R D P A R T Y . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2020 United States Government as represented by
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

# Synopsis:
#
# THIRD_PARTY(dir
#             vroot
#             build_target
#             description
#             [FIND_NAME name]
#             [FIND_VERSION version]
#             [FIND_COMPONENTS component1 component2 ...]
#             [REQUIRED_VARS var1 var2 ...]
#             [RESET_VARS var1 var2 ...]
#             [ALIASES alias1 alias2 ...]
#             [FLAGS flag1 flag2 ...]
#             [UNDOCUMENTED]
#            )

#-----------------------------------------------------------------------------
function(THIRD_PARTY dir vroot build_target description)

  # Parse extra arguments
  cmake_parse_arguments(TP "UNDOCUMENTED;NOSYS" "FIND_NAME;FIND_VERSION" "FIND_COMPONENTS;REQUIRED_VARS;RESET_VARS;ALIASES;FLAGS" ${ARGN})

  # If the library variable has been explicitly set, get
  # a normalized version of it for easier matching
  set(local_opt)
  if(NOT "${${CMAKE_PROJECT_NAME}_${vroot}}" STREQUAL "")
    set(local_opt "${${CMAKE_PROJECT_NAME}_${vroot}}")
  endif(NOT "${${CMAKE_PROJECT_NAME}_${vroot}}" STREQUAL "")
  VAL_NORMALIZE(local_opt ABS)

  # Initialize some variables
  set(TP_DISABLED 0)
  set(TP_DISABLE_TEST 0)
  set(TP_MET_CONDITION 0)
  if(NOT TP_FIND_NAME)
    set(TP_FIND_NAME ${vroot})
  endif(NOT TP_FIND_NAME)

  # 0. Whether or not we're building the sources, we are tracking the files
  # that are supposed to be in the directory
  get_filename_component(DIR_NAME "${dir}" NAME)
  #if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist")
  #  message(FATAL_ERROR "Third party component ${DIR_NAME} does not have a dist file at \"${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist\"")
  #endif(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist")
  #include("${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist")
  #CMAKEFILES_IN_DIR(${DIR_NAME}_ignore_files ${dir})

  # 1. If any of the required flags are off, this extension is a no-go.
  set(DISABLE_STR "")
  foreach(item ${TP_REQUIRED_VARS})
    if(NOT ${item})
      set(TP_DISABLED 1)
      set(TP_DISABLE_TEST 1)
      set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD OFF)
      if(NOT DISABLE_STR)
	set(DISABLE_STR "${item}")
      else(NOT DISABLE_STR)
	set(DISABLE_STR "${DISABLE_STR},${item}")
      endif(NOT DISABLE_STR)
    endif(NOT ${item})
  endforeach(item ${TP_REQUIRED_VARS})
  if(DISABLE_STR)
    set(${CMAKE_PROJECT_NAME}_${vroot} "DISABLED ${DISABLE_STR}" CACHE STRING "DISABLED ${DISABLED_STR}" FORCE)
    mark_as_advanced(FORCE ${CMAKE_PROJECT_NAME}_${vroot})
    set_property(CACHE ${CMAKE_PROJECT_NAME}_${vroot} PROPERTY STRINGS "DISABLED ${DISABLE_STR}")
    set(TP_MET_CONDITION 1)
  else(DISABLE_STR)
    # If we have a leftover disabled setting in the cache from earlier runs, clear it.
    if("${local_opt}" MATCHES "DISABLED")
      set(${CMAKE_PROJECT_NAME}_${vroot} "" CACHE STRING "Clear DISABLED setting" FORCE)
      mark_as_advanced(CLEAR ${CMAKE_PROJECT_NAME}_${vroot})
    endif("${local_opt}" MATCHES "DISABLED")
  endif(DISABLE_STR)

  # 2. Next - is the library variable explicitly set to SYSTEM?  If it is, we are NOT building it.
  if(local_opt)
    if("${local_opt}" STREQUAL "SYSTEM")
      set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD OFF)
      set(${CMAKE_PROJECT_NAME}_${vroot} "SYSTEM" CACHE STRING "User forced to SYSTEM" FORCE)
      set(TP_MET_CONDITION 2)
      if(TP_NOSYS)
	  message(WARNING "Compilation of ${build_target} was disabled, but local copy is modified - using a system version of ${build_target} may introduce problems or even fail to work!")
      endif(TP_NOSYS)
    endif("${local_opt}" STREQUAL "SYSTEM")
  endif(local_opt)

  # 3. If we have a NOSYS flag and aren't explicitly disabled, ALWAYS use the bundled version.
  # The NOSYS flag signifies that the BRL-CAD project requires modifications in the local src/other
  # version of a library or tool that are not likely to be present in a system version.  These flags
  # should be periodically reviewed to determine if they can be removed (i.e. system packages have
  # appeared in modern OS distributions with the fixes needed by BRL-CAD...)
  if(NOT TP_MET_CONDITION AND TP_NOSYS)
    set(${CMAKE_PROJECT_NAME}_${vroot} "BUNDLED" CACHE STRING "NOSYS passed, using bundled ${build_target}" FORCE)
    mark_as_advanced(${CMAKE_PROJECT_NAME}_${vroot})
    set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD ON)
    set(TP_MET_CONDITION 3)
    set(TP_DISABLE_TEST 1)
  endif(NOT TP_MET_CONDITION AND TP_NOSYS)

  # 4. If we have an explicit BUNDLE request for this particular library,  honor it as long as
  # features are satisfied.  No point in testing if we know we're turning it on - set vars accordingly.
  if(local_opt)
    if("${local_opt}" STREQUAL "BUNDLED")
      if(NOT TP_MET_CONDITION)
	set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${vroot} "BUNDLED" CACHE STRING "User forced to BUNDLED" FORCE)
	set(TP_DISABLE_TEST 1)
	set(TP_MET_CONDITION 4)
      endif(NOT TP_MET_CONDITION)
    endif("${local_opt}" STREQUAL "BUNDLED")
  endif(local_opt)

  # 5. If ${CMAKE_PROJECT_NAME}_BUNDLED_LIBS is exactly SYSTEM or exactly BUNDLED, and we haven't been overridden by
  # one of the other conditions above, go with that.
  if(NOT TP_MET_CONDITION)
    if("${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "BUNDLED")
      set(${CMAKE_PROJECT_NAME}_${vroot} "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} (AUTO)" CACHE
       	STRING "${CMAKE_PROJECT_NAME}_BUNDLED_LIBS: ${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" FORCE)
      if("${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD OFF)
	set(TP_MET_CONDITION 5)
      elseif("${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "BUNDLED")
	set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD ON)
	set(TP_DISABLE_TEST 1)
	set(TP_MET_CONDITION 5)
      endif("${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM")
    endif("${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "BUNDLED")
  endif(NOT TP_MET_CONDITION)

  # If we haven't been knocked out by any of the above conditions, do our testing and base the results on that.

  if(NOT TP_DISABLE_TEST)
    # Stash the previous results (if any) so we don't repeatedly call out the tests - only report
    # if something actually changes in subsequent runs.
    set(${vroot}_FOUND_STATUS ${${vroot}_FOUND})

    # Initialize (or rather, uninitialize) variables in preparation for search
    set(${vroot}_FOUND "${vroot}-NOTFOUND" CACHE STRING "${vroot}_FOUND" FORCE)
    mark_as_advanced(${vroot}_FOUND)
    foreach(item ${TP_RESET_VARS})
      set(${item} "${vroot}-NOTFOUND" CACHE STRING "${item}" FORCE)
    endforeach(item ${TP_RESET_VARS})

    # Be quiet if we're doing this over
    if("${${vroot}_FOUND_STATUS}" MATCHES "NOTFOUND")
      set(${vroot}_FIND_QUIETLY TRUE)
    endif("${${vroot}_FOUND_STATUS}" MATCHES "NOTFOUND")

    # Find the package in question.  It is the toplevel CMakeLists.txt's responsibility to make
    # sure that the CMAKE_MODULE_PATH variable is set correctly if there are local versions of
    # Find*.cmake scripts that should be used.  Note that newer CMake versions will prefer a system
    # version of the module, so if a custom override is needed the Find*.cmake name should not conflict
    # with the system version.
    find_package(${TP_FIND_NAME} ${TP_FIND_VERSION} COMPONENTS ${TP_FIND_COMPONENTS})

    # going to use system or bundled versions of deps
    if(${vroot}_FOUND)
      set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD OFF)
      if(NOT "${CMAKE_PROJECT_NAME}_${vroot}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${vroot} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
      endif(NOT "${CMAKE_PROJECT_NAME}_${vroot}" STREQUAL "SYSTEM")
    else(${vroot}_FOUND)
      # If one of our previous conditions precludes building this library, we've got a problem.
      if("${local_opt}" STREQUAL "SYSTEM" OR "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD OFF)
	set(${CMAKE_PROJECT_NAME}_${vroot}_NOTFOUND 1)
	message(WARNING "Compilation of local version of ${build_target} was disabled, but system version not found!")
	if(NOT "${local_opt}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${vroot} "SYSTEM (AUTO)" CACHE STRING "${CMAKE_PROJECT_NAME}_BUNDLED_LIBS forced to SYSTEM, but library not found!" FORCE)
	else(NOT "${local_opt}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${vroot} "SYSTEM" CACHE STRING "Hard-set to SYSTEM by user, but library not found!" FORCE)
	endif(NOT "${local_opt}" STREQUAL "SYSTEM")
      else("${local_opt}" STREQUAL "SYSTEM" OR "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${vroot} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
      endif("${local_opt}" STREQUAL "SYSTEM" OR "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL SYSTEM)
    endif(${vroot}_FOUND)
  endif(NOT TP_DISABLE_TEST)

  # If we're going with a system version of a library, we have to remove any previously built
  # output from enabled bundled copies of the library in question, or the linker will get
  # confused - a system lib was found and system libraries are to be preferred with current
  # options.  This is unfortunate in that it may introduce extra build work just from trying
  # configure options, but appears to be essential to ensuring that the build "just works"
  # each time.
  if (NOT ${CMAKE_PROJECT_NAME}_${vroot}_BUILD)

    if (EXISTS "${CMAKE_BUILD_DIR}/${build_target}_files.txt")

      file(STRINGS "${CMAKE_BUILD_DIR}/${build_target}_files.txt" STALE_FILES)

      foreach(stale_file ${STALE_FILES})
	execute_process(
	  COMMAND ${CMAKE_COMMAND} -E remove ${stale_file}
	  OUTPUT_VARIABLE rm_out
	  RETURN_VALUE rm_retval
	  )
      endforeach(stale_file ${STALE_FILES})

    endif (EXISTS "${CMAKE_BUILD_DIR}/${build_target}_files.txt")

    if(NOT "${CMAKE_PROJECT_NAME}_${vroot}" MATCHES "${local_opt}")
      if("${local_opt}" MATCHES "BUNDLED")
	message("Reconfiguring to use system ${vroot}")
      endif("${local_opt}" MATCHES "BUNDLED")
    endif(NOT "${CMAKE_PROJECT_NAME}_${vroot}" MATCHES "${local_opt}")

  endif(NOT ${CMAKE_PROJECT_NAME}_${vroot}_BUILD)

  # After all that, we know now what to do about this particular dependency
  set(${CMAKE_PROJECT_NAME}_${vroot}_BUILD "${${CMAKE_PROJECT_NAME}_${vroot}_BUILD}" PARENT_SCOPE)

  # Unless we're suppressing it, document the options that control this third party component
  if(NOT ${vroot}_UNDOCUMENTED)
    BRLCAD_OPTION("${CMAKE_PROJECT_NAME}_${vroot}" "${${CMAKE_PROJECT_NAME}_${vroot}}"
      TYPE ABS
      ALIASES ${TP_ALIASES}
      DESCRIPTION "${description}")
  endif(NOT ${vroot}_UNDOCUMENTED)

  # For drop-down menus in CMake gui - set STRINGS property
  set_property(CACHE ${CMAKE_PROJECT_NAME}_${vroot} PROPERTY STRINGS AUTO BUNDLED SYSTEM)

  mark_as_advanced(${vroot}_LIBRARY)
  mark_as_advanced(${vroot}_INCLUDE_DIR)
endfunction(THIRD_PARTY)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
