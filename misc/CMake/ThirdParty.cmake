#                T H I R D P A R T Y . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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

include(CMakeParseArguments)

# Synopsis:
#
# THIRD_PARTY(dir
#             varname_root
#             build_target
#             description
#             [FIND_NAME name]
#             [FIND_VERSION version]
#             [FIND_COMPONENTS component1 component2 ...]
#             [REQUIRED_VARS var1 var2 ...]
#             [ALIASES alias1 alias2 ...]
#             [FLAGS flag1 flag2 ...]
#            )

#-----------------------------------------------------------------------------
macro(THIRD_PARTY dir varname_root build_target description)

  # If the library variable has been explicitly set, get
  # an varname_rootcase version of it for easier matching
  if(NOT ${${CMAKE_PROJECT_NAME}_${varname_root}} STREQUAL "")
    string(TOUPPER "${${CMAKE_PROJECT_NAME}_${varname_root}}" OPT_STR_UPPER)
  else(NOT ${${CMAKE_PROJECT_NAME}_${varname_root}} STREQUAL "")
    set(OPT_STR_UPPER "")
  endif(NOT ${${CMAKE_PROJECT_NAME}_${varname_root}} STREQUAL "")
  if(NOT ${OPT_STR_UPPER} STREQUAL "")
    if(${OPT_STR_UPPER} STREQUAL "ON")
      set(OPT_STR_UPPER "BUNDLED")
    endif(${OPT_STR_UPPER} STREQUAL "ON")
    if(${OPT_STR_UPPER} STREQUAL "OFF")
      set(OPT_STR_UPPER "SYSTEM")
    endif(${OPT_STR_UPPER} STREQUAL "OFF")
  endif(NOT ${OPT_STR_UPPER} STREQUAL "")

  if(${ARGC} GREATER 3)
    # Parse extra arguments
    CMAKE_PARSE_ARGUMENTS(${varname_root} "" "" "FIND_NAME;FIND_VERSION;FIND_COMPONENTS;REQUIRED_VARS;ALIASES;FLAGS" ${ARGN})
  endif(${ARGC} GREATER 3)
  if(NOT ${varname_root}_FIND_NAME)
    set(${varname_root}_FIND_NAME ${varname_root})
  endif(NOT ${varname_root}_FIND_NAME)

  # Initialize some variables
  set(${varname_root}_DISABLED 0)
  set(${varname_root}_DISABLE_TEST 0)
  set(${varname_root}_MET_CONDITION 0)

  # 1. If any of the required flags are off, this extension is a no-go.
  set(DISABLE_STR "")
  foreach(item ${${varname_root}_REQUIRED_VARS})
    if(NOT ${item})
      set(${varname_root}_DISABLED 1)
      set(${varname_root}_DISABLE_TEST 1)
      set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
      if(NOT DISABLE_STR)
	set(DISABLE_STR "${item}")
      else(NOT DISABLE_STR)
	set(DISABLE_STR "${DISABLE_STR},${item}")
      endif(NOT DISABLE_STR)
    endif(NOT ${item})
  endforeach(item ${${varname_root}_REQUIRED_VARS})
  if(DISABLE_STR)
    set(${CMAKE_PROJECT_NAME}_${varname_root} "DISABLED ${DISABLE_STR}" CACHE STRING "DISABLED ${DISABLED_STR}" FORCE)
    mark_as_advanced(FORCE ${CMAKE_PROJECT_NAME}_${varname_root})
    set_property(CACHE ${CMAKE_PROJECT_NAME}_${varname_root} PROPERTY STRINGS "DISABLED ${DISABLE_STR}")
    set(${varname_root}_MET_CONDITION 1)
  else(DISABLE_STR)
    # If we have a leftover disabled setting in the cache from earlier runs, clear it.
    if("${CMAKE_PROJECT_NAME}_${varname_root}" MATCHES "DISABLED")
      set(${CMAKE_PROJECT_NAME}_${varname_root} "" CACHE STRING "Clear DISABLED setting" FORCE)
      mark_as_advanced(CLEAR ${CMAKE_PROJECT_NAME}_${varname_root})
    endif("${CMAKE_PROJECT_NAME}_${varname_root}" MATCHES "DISABLED")
  endif(DISABLE_STR)

  # 2. Next - is the library variable explicitly set to SYSTEM?  If it is, we are NOT building it.
  if(OPT_STR_UPPER)
    if("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
      set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
      set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM" CACHE STRING "User forced to SYSTEM" FORCE)
      set(${varname_root}_MET_CONDITION 2)
      foreach(extraarg ${${varname_root}_FLAGS})
	if(extraarg STREQUAL "NOSYS")
	  message(WARNING "Compilation of ${build_target} was disabled, but local copy is modified - using a system version of ${build_target} may introduce problems or even fail to work!")

	endif(extraarg STREQUAL "NOSYS")
      endforeach(extraarg ${${varname_root}_FLAGS})
    endif("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
  endif(OPT_STR_UPPER)

  # 3. If we have a NOSYS flag and aren't explicitly disabled, ALWAYS use the bundled version.
  # The NOSYS flag signifies that the BRL-CAD project requires modifications in the local src/other
  # version of a library or tool that are not likely to be present in a system version.  These flags
  # should be periodically reviewed to determine if they can be removed (i.e. system packages have
  # appeared in modern OS distributions with the fixes needed by BRL-CAD...)
  if(NOT ${varname_root}_MET_CONDITION)
    foreach(extraarg ${${varname_root}_FLAGS})
      if(extraarg STREQUAL "NOSYS")
	set(${CMAKE_PROJECT_NAME}_${varname_root} "BUNDLED" CACHE STRING "NOSYS passed, using bundled ${build_target}" FORCE)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(${varname_root}_MET_CONDITION 3)
	set(${varname_root}_DISABLE_TEST 1)
      endif(extraarg STREQUAL "NOSYS")
    endforeach(extraarg ${${varname_root}_FLAGS})
  endif(NOT ${varname_root}_MET_CONDITION)

  # 4. If we have an explicit BUNDLE request for this particular library,  honor it as long as
  # features are satisfied.  No point in testing if we know we're turning it on - set vars accordingly.
  if(OPT_STR_UPPER)
    if("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
      if(NOT ${varname_root}_MET_CONDITION)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${varname_root} "BUNDLED" CACHE STRING "User forced to BUNDLED" FORCE)
	set(${varname_root}_DISABLE_TEST 1)
	set(${varname_root}_MET_CONDITION 4)
      endif(NOT ${varname_root}_MET_CONDITION)
    endif("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
  endif(OPT_STR_UPPER)


  # 5. If BRLCAD_BUNDLED_LIBS is exactly SYSTEM or exactly BUNDLED, and we haven't been overridden by
  # one of the other conditions above, go with that.
  if(NOT ${varname_root}_MET_CONDITION)
    if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
      set(${CMAKE_PROJECT_NAME}_${varname_root} "${BRLCAD_BUNDLED_LIBS} (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS: ${BRLCAD_BUNDLED_LIBS}" FORCE)
      if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
	set(${varname_root}_MET_CONDITION 5)
      ELSEif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(${varname_root}_DISABLE_TEST 1)
	set(${varname_root}_MET_CONDITION 5)
      endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
    endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
  endif(NOT ${varname_root}_MET_CONDITION)


  # If we haven't been knocked out by any of the above conditions, do our testing and base the results on that.

  if(NOT ${varname_root}_DISABLE_TEST)
    # Stash the previous results (if any) so we don't repeatedly call out the tests - only report
    # if something actually changes in subsequent runs.
    set(${varname_root}_FOUND_STATUS ${${varname_root}_FOUND})

    # Initialize (or rather, uninitialize) variables in preparation for search
    set(${varname_root}_FOUND "${varname_root}-NOTFOUND" CACHE STRING "${varname_root}_FOUND" FORCE)
    mark_as_advanced(${varname_root}_FOUND)
    set(${varname_root}_LIBRARY "${varname_root}-NOTFOUND" CACHE STRING "${varname_root}_LIBRARY" FORCE)
    set(${varname_root}_INCLUDE_DIR "${varname_root}-NOTFOUND" CACHE STRING "${varname_root}_INCLUDE_DIR" FORCE)

    # Be quiet if we're doing this over
    if("${${varname_root}_FOUND_STATUS}" MATCHES "NOTFOUND")
      set(${varname_root}_FIND_QUIETLY TRUE)
    endif("${${varname_root}_FOUND_STATUS}" MATCHES "NOTFOUND")

    # Find the package in question.  It is the toplevel CMakeLists.txt's responsibility to make
    # sure that the CMAKE_MODULE_PATH variable is set correctly if there are local versions of
    # Find*.cmake scripts that should be used instead of the installed CMake version.

    find_package(${${varname_root}_FIND_NAME} ${${varname_root}_FIND_VERSION} COMPONENTS ${${varname_root}_FIND_COMPONENTS})

    # going to use system or bundled versions of deps
    if(${varname_root}_FOUND)
      set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
      if(NOT "${CMAKE_PROJECT_NAME}_${varname_root}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
      endif(NOT "${CMAKE_PROJECT_NAME}_${varname_root}" STREQUAL "SYSTEM")
    else(${varname_root}_FOUND)
      # If one of our previous conditions precludes building this library, we've got a problem.
      if("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_NOTFOUND 1)
	message(WARNING "Compilation of local version of ${build_target} was disabled, but system version not found!")
	if(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS forced to SYSTEM, but library not found!" FORCE)
	else(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM" CACHE STRING "Hard-set to SYSTEM by user, but library not found!" FORCE)
	endif(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
      else("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${varname_root} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
      endif("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
    endif(${varname_root}_FOUND)
  endif(NOT ${varname_root}_DISABLE_TEST)

  # If we're going with a system version of a library, we have to remove any previously built
  # output from enabled bundled copies of the library in question, or the linker will get
  # confused - a system lib was found and system libraries are to be preferred with current
  # options.  This is unfortunate in that it may introduce extra build work just from trying
  # configure options, but appears to be essential to ensuring that the build "just works"
  # each time.
  if(NOT ${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)
    string(REGEX REPLACE "lib" "" rootname "${build_target}")
    file(GLOB STALE_FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${rootname}*${CMAKE_SHARED_LIBRARY_SUFFIX}*")

    foreach(stale_file ${STALE_FILES})
      EXEC_PROGRAM(
	${CMAKE_COMMAND} ARGS -E remove ${stale_file}
	OUTPUT_VARIABLE rm_out
	RETURN_VALUE rm_retval
	)
    endforeach(stale_file ${STALE_FILES})

    if(NOT "${CMAKE_PROJECT_NAME}_${varname_root}" MATCHES "${OPT_STR_UPPER}")
      if("${OPT_STR_UPPER}" MATCHES "BUNDLED")
	message("Reconfiguring to use system ${varname_root}")
      endif("${OPT_STR_UPPER}" MATCHES "BUNDLED")
    endif(NOT "${CMAKE_PROJECT_NAME}_${varname_root}" MATCHES "${OPT_STR_UPPER}")
  endif(NOT ${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)

  # After all that, we know now what to do about src/other/${dir} - do it
  if(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)
    add_subdirectory(${dir})
    set(${varname_root}_LIBRARY "${build_target}" CACHE STRING "${varname_root}_LIBRARY" FORCE)
    set(${varname_root}_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${dir};${CMAKE_CURRENT_BINARY_DIR}/${dir} CACHE STRING "set by THIRD_PARTY_SUBDIR macro" FORCE)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
      include(${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
      CMAKEFILES_IN_DIR(${dir}_ignore_files ${dir})
    else(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
      message("Bundled build, but file ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist not found")
    endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
  else(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)
    CMAKEFILES(${dir})
  endif(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)

  OPTION_ALIASES("${CMAKE_PROJECT_NAME}_${varname_root}" "${varname_root}_ALIASES" "ABS")
  OPTION_DESCRIPTION("${CMAKE_PROJECT_NAME}_${varname_root}" "${varname_root}_ALIASES" "${description}")

  # For drop-down menus in CMake gui - set STRINGS property
  set_property(CACHE ${CMAKE_PROJECT_NAME}_${varname_root} PROPERTY STRINGS AUTO BUNDLED SYSTEM)

  mark_as_advanced(${varname_root}_LIBRARY)
  mark_as_advanced(${varname_root}_INCLUDE_DIR)
endmacro(THIRD_PARTY)


#-----------------------------------------------------------------------------
macro(THIRD_PARTY_EXECUTABLE lower dir required_vars aliases description)
  string(TOUPPER ${lower} upper)
  # If the exec variable has been explicitly set, get
  # an uppercase version of it for easier matching
  if(NOT ${${CMAKE_PROJECT_NAME}_${upper}} STREQUAL "")
    string(TOUPPER "${${CMAKE_PROJECT_NAME}_${upper}}" OPT_STR_UPPER)
  else(NOT ${${CMAKE_PROJECT_NAME}_${upper}} STREQUAL "")
    set(OPT_STR_UPPER "")
  endif(NOT ${${CMAKE_PROJECT_NAME}_${upper}} STREQUAL "")
  if(NOT ${OPT_STR_UPPER} STREQUAL "")
    if(${OPT_STR_UPPER} STREQUAL "ON")
      set(OPT_STR_UPPER "BUNDLED")
    endif(${OPT_STR_UPPER} STREQUAL "ON")
    if(${OPT_STR_UPPER} STREQUAL "OFF")
      set(OPT_STR_UPPER "SYSTEM")
    endif(${OPT_STR_UPPER} STREQUAL "OFF")
  endif(NOT ${OPT_STR_UPPER} STREQUAL "")

  # For executables, it is a reasonable use case that the developer manually specifies
  # the location for an executable.  It is tricky to distinguish this situation from
  # a previously cached executable path resulting from a Find*.cmake script.  The way
  # we will proceed is to cache the value of ${upper}_EXECUTABLE if it is defined, and
  # at the end check it against the results of running the THIRD_PARTY logic.  If
  # it matches neither pattern (Bundled or System) it is assumed that the value passed
  # in is an intentional setting on the part of the developer.  This has one potential
  # drawback in that the *removal* of a system executable between searches could result
  # in a previously cached system search result being identified as a user-specified
  # result - to prevent that, the cached path is only used to override other results
  # if the file it specifies actually exists.  One additional wrinkle here - if the
  # developer has hard-specified BUNDLED for this particular executable, even a user specified
  # or cached value will be replaced with the local path.  A feature based disablement
  # of the tool also applies to the cached version.
  if(${upper}_EXECUTABLE)
    IS_SUBPATH("${CMAKE_BINARY_DIR}" "${${upper}_EXECUTABLE}" SUBBUILD_TEST)
    if("${SUBBUILD_TEST}" STREQUAL "0")
      get_filename_component(FULL_PATH_EXEC ${${upper}_EXECUTABLE} ABSOLUTE)
      if("${FULL_PATH_EXEC}" STREQUAL "${${upper}_EXECUTABLE}")
	if(EXISTS ${FULL_PATH_EXEC})
	  set(EXEC_CACHED ${${upper}_EXECUTABLE})
	else(EXISTS ${FULL_PATH_EXEC})
	  # This path not being present may indicate the user specified a path
	  # and made a mistake doing so - warn that this might be the case.
	  message(WARNING "File path ${${upper}_EXECUTABLE} specified for ${upper}_EXECUTABLE does not exist - this path will not override ${lower} executable search results.")
	endif(EXISTS ${FULL_PATH_EXEC})
      endif("${FULL_PATH_EXEC}" STREQUAL "${${upper}_EXECUTABLE}")
    endif("${SUBBUILD_TEST}" STREQUAL "0")
  endif(${upper}_EXECUTABLE)

  # Initialize some variables
  set(${upper}_DISABLED 0)
  set(${upper}_DISABLE_TEST 0)
  set(${upper}_MET_CONDITION 0)

  # 1. If any of the required flags are off, this tool is a no-go.
  set(DISABLE_STR "")
  foreach(item ${required_vars})
    if(NOT ${item})
      set(${upper}_DISABLED 1)
      set(${upper}_DISABLE_TEST 1)
      set(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
      if(NOT DISABLE_STR)
	set(DISABLE_STR "${item}")
      else(NOT DISABLE_STR)
	set(DISABLE_STR "${DISABLE_STR},${item}")
      endif(NOT DISABLE_STR)
    endif(NOT ${item})
  endforeach(item ${required_vars})
  if(DISABLE_STR)
    set(${CMAKE_PROJECT_NAME}_${upper} "DISABLED ${DISABLE_STR}" CACHE STRING "DISABLED ${DISABLED_STR}" FORCE)
    mark_as_advanced(FORCE ${CMAKE_PROJECT_NAME}_${upper})
    set(${upper}_MET_CONDITION 1)
  else(DISABLE_STR)
    # If we have a leftover disabled setting in the cache from earlier runs, clear it.
    if("${CMAKE_PROJECT_NAME}_${upper}" MATCHES "DISABLED")
      set(${CMAKE_PROJECT_NAME}_${upper} "" CACHE STRING "Clear DISABLED setting" FORCE)
      mark_as_advanced(CLEAR ${CMAKE_PROJECT_NAME}_${upper})
    endif("${CMAKE_PROJECT_NAME}_${upper}" MATCHES "DISABLED")
  endif(DISABLE_STR)

  # 2. Next - is the executable variable explicitly set to SYSTEM?  If it is, we are NOT building it.
  if(OPT_STR_UPPER)
    if("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
      set(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
      set(${upper}_MET_CONDITION 2)
      foreach(extraarg ${ARGN})
	if(extraarg STREQUAL "NOSYS")
	  message(WARNING "Compilation of ${lower} was disabled, but local copy is modified - using a system version of ${lower} may introduce problems or even fail to work!")

	endif(extraarg STREQUAL "NOSYS")
      endforeach(extraarg ${ARGN})
    endif("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
  endif(OPT_STR_UPPER)

  # 3. If we have a NOSYS flag, ALWAYS* use the bundled version.  The NOSYS flag signifies that
  # the BRL-CAD project requires modifications in the local src/other version of a library or
  # tool that are not likely to be present in a system version.  These flags should be periodically
  # reviewed to determine if they can be removed (i.e. system packages have appeared in modern
  # OS distributions with the fixes needed by BRL-CAD...).
  #
  # * In the case of executables, we'll allow a cached value from the user to override the NOSYS
  # flag, since that's a likely scenario for testing, but that shouldn't be done except for testing
  # purposes with a NOSYS target.
  if(NOT ${upper}_MET_CONDITION)
    foreach(extraarg ${ARGN})
      if(extraarg STREQUAL "NOSYS")
	set(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED" CACHE STRING "NOSYS passed, using bundled ${lower}" FORCE)
	set(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
	set(${upper}_MET_CONDITION 3)
	set(${upper}_DISABLE_TEST 1)
      endif(extraarg STREQUAL "NOSYS")
    endforeach(extraarg ${ARGN})
  endif(NOT ${upper}_MET_CONDITION)

  # 4. If we have an explicit BUNDLE request for this particular executable,  honor it as long as
  # features are satisfied.  No point in testing if we know we're turning it on - set vars accordingly.
  if(OPT_STR_UPPER)
    if("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
      if(NOT ${upper}_MET_CONDITION)
	set(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
	set(${upper}_DISABLE_TEST 1)
	set(${upper}_MET_CONDITION 4)
      endif(NOT ${upper}_MET_CONDITION)
    endif("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
  endif(OPT_STR_UPPER)


  # 5. If BRLCAD_BUNDLED_LIBS is exactly SYSTEM or exactly BUNDLED, and we haven't been overridden by
  # one of the other conditions above, go with that.
  if(NOT ${upper}_MET_CONDITION)
    if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
      set(${CMAKE_PROJECT_NAME}_${upper} "${BRLCAD_BUNDLED_LIBS} (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS: ${BRLCAD_BUNDLED_LIBS}" FORCE)
      if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	set(${upper}_MET_CONDITION 5)
      ELSEif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
	set(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
	set(${upper}_DISABLE_TEST 1)
	set(${upper}_MET_CONDITION 5)
      endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
    endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
  endif(NOT ${upper}_MET_CONDITION)

  # If we haven't been knocked out by any of the above conditions, do our testing and base the results on that.
  if(NOT ${upper}_DISABLE_TEST)
    # Stash the previous results (if any) so we don't repeatedly call out the tests - only report
    # if something actually changes in subsequent runs.
    set(${upper}_FOUND_STATUS ${${upper}_FOUND})

    # Initialize (or rather, uninitialize) variables in preparation for search
    set(${upper}_FOUND "${upper}-NOTFOUND" CACHE STRING "${upper}_FOUND" FORCE)
    mark_as_advanced(${upper}_FOUND)
    set(${upper}_EXECUTABLE "${upper}-NOTFOUND" CACHE STRING "${upper}_EXECUTABLE" FORCE)

    # Be quiet if we're doing this over
    if("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")
      set(${upper}_FIND_QUIETLY TRUE)
    endif("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")

    # Include the Find module for the library in question
    if(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
      include(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
    else(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
      include(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
    endif(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)

    # going to use system or bundled versions of deps
    if(${upper}_FOUND)
      set(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
      set(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
    else(${upper}_FOUND)
      # If one of our previous conditions precludes building this library, we've got a problem unless the
      # cached version is suitable - check that before we warn.
      if(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM" AND NOT "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
      endif(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM" AND NOT "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
    endif(${upper}_FOUND)
  endif(NOT ${upper}_DISABLE_TEST)


  # Now that we've run the Find routine, see if we had a cached value different from any of our
  # standard results
  set(${upper}_USING_CACHED 0)
  if(NOT "${upper}_MET_CONDITION" STREQUAL "1" AND NOT "${upper}_MET_CONDITION" STREQUAL "4")
    if(EXEC_CACHED)
      # Is it a build target?  If so, don't cache it.
      get_filename_component(EXEC_CACHED_ABS_PATH ${EXEC_CACHED} ABSOLUTE)
      IF ("${EXEC_CACHED_ABS_PATH}" STREQUAL "${PATH_ABS}")
	# Is it the bundled path? (don't override if it is, the bundled option setting takes care of that)
	if(NOT "${EXEC_CACHED}" STREQUAL "${CMAKE_BINARY_DIR}/${BIN_DIR}/${lower}")
	  # Is it the same as the found results?
	  if(NOT "${EXEC_CACHED}" STREQUAL "${${upper}_EXECUTABLE_FOUND_RESULT}")
	    set(${upper}_EXECUTABLE ${EXEC_CACHED} CACHE STRING "Apparently a user specified path was supplied, use it" FORCE)
	    set(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    set(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Apparently a user specified path was supplied, use it" FORCE)
	    set(${upper}_USING_CACHED 1)
	  endif(NOT "${EXEC_CACHED}" STREQUAL "${${upper}_EXECUTABLE_FOUND_RESULT}")
	endif(NOT "${EXEC_CACHED}" STREQUAL "${CMAKE_BINARY_DIR}/${BIN_DIR}/${lower}")
      endif("${EXEC_CACHED_ABS_PATH}" STREQUAL "${PATH_ABS}")
    endif(EXEC_CACHED)
  endif(NOT "${upper}_MET_CONDITION" STREQUAL "1" AND NOT "${upper}_MET_CONDITION" STREQUAL "4")

  # If the CACHED value doesn't look good, and we aren't overridden by any of the other conditions, set based
  # on the test results
  if(NOT ${upper}_USING_CACHED AND NOT ${upper}_DISABLE_TEST)
    if(${upper}_FOUND)
      set(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
      if(NOT "${CMAKE_PROJECT_NAME}_${upper}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
      endif(NOT "${CMAKE_PROJECT_NAME}_${upper}" STREQUAL "SYSTEM")
    else(${upper}_FOUND)
      # If one of our previous conditions precludes building this exec, we've got a problem.
      if("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${upper}_NOTFOUND 1)
	message(WARNING "Compilation of local version of ${lower} was disabled, but system version not found!")
	if(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS forced to SYSTEM, but library not found!" FORCE)
	else(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM" CACHE STRING "Hard-set to SYSTEM by user, but library not found!" FORCE)
	endif(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
      else("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
      endif("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
    endif(${upper}_FOUND)
  endif(NOT ${upper}_USING_CACHED AND NOT ${upper}_DISABLE_TEST)


  if(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
    # In the case of executables, it is possible that a directory may define more than one
    # executable target.  If this is the case, we may have already added this directory -
    # if so, don't do it again.
    if(SRC_OTHER_ADDED_DIRS)
      list(FIND SRC_OTHER_ADDED_DIRS ${dir} ADDED_RESULT)
      if("${ADDED_RESULT}" STREQUAL "-1")
	add_subdirectory(${dir})
	set(SRC_OTHER_ADDED_DIRS ${SRC_OTHER_ADDED_DIRS} ${dir})
	list(REMOVE_DUPLICATES SRC_OTHER_ADDED_DIRS)
	set(SRC_OTHER_ADDED_DIRS ${SRC_OTHER_ADDED_DIRS} CACHE STRING "Enabled 3rd party sub-directories" FORCE)
	mark_as_advanced(SRC_OTHER_ADDED_DIRS)
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	  include(${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	  CMAKEFILES_IN_DIR(${dir}_ignore_files ${dir})
	else(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	  message("Bundled build, but file ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist not found")
	endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
      endif("${ADDED_RESULT}" STREQUAL "-1")
    else(SRC_OTHER_ADDED_DIRS)
      add_subdirectory(${dir})
      set(SRC_OTHER_ADDED_DIRS ${dir} CACHE STRING "Enabled 3rd party sub-directories" FORCE)
      mark_as_advanced(SRC_OTHER_ADDED_DIRS)
      if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	include(${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	CMAKEFILES_IN_DIR(${dir}_ignore_files ${dir})
      else(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	message("Bundled build, but file ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist not found")
      endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
    endif(SRC_OTHER_ADDED_DIRS)
    if(CMAKE_CONFIGURATION_TYPES)
      set(${upper}_EXECUTABLE "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${BIN_DIR}/${lower}" CACHE STRING "${upper}_EXECUTABLE" FORCE)
    else(CMAKE_CONFIGURATION_TYPES)
      set(${upper}_EXECUTABLE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${lower}" CACHE STRING "${upper}_EXECUTABLE" FORCE)
    endif(CMAKE_CONFIGURATION_TYPES)
    set(${upper}_EXECUTABLE_TARGET ${lower} CACHE STRING "Build target for ${lower}" FORCE)
  else(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
    CMAKEFILES(${dir})
    set(${upper}_EXECUTABLE_TARGET "" CACHE STRING "No build target for ${lower}" FORCE)
  endif(${CMAKE_PROJECT_NAME}_${upper}_BUILD)

  OPTION_ALIASES("${CMAKE_PROJECT_NAME}_${upper}" "${aliases}" "ABS")
  OPTION_DESCRIPTION("${CMAKE_PROJECT_NAME}_${upper}" "${aliases}" "${description}")

  # For drop-down menus in CMake gui - set STRINGS property
  set_property(CACHE ${CMAKE_PROJECT_NAME}_${upper} PROPERTY STRINGS AUTO BUNDLED SYSTEM)

  mark_as_advanced(${upper}_EXECUTABLE)
  mark_as_advanced(${upper}_EXECUTABLE_TARGET)
  mark_as_advanced(${upper}_FOUND)
endmacro(THIRD_PARTY_EXECUTABLE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
