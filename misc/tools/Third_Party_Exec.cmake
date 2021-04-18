function(THIRD_PARTY_EXECUTABLE dir varname_root build_target)

  # Parse extra arguments
  cmake_parse_arguments(TP "NOSYS" "" "REQUIRED_VARS;ALIASES;DESCRIPTION" ${ARGN})

  # If the exec variable has been explicitly set, get
  # an normalized version of it for easier matching
  set(local_opt)
  if(NOT ${${CMAKE_PROJECT_NAME}_${varname_root}} STREQUAL "")
    set(local_opt "${${CMAKE_PROJECT_NAME}_${varname_root}}")
  endif(NOT ${${CMAKE_PROJECT_NAME}_${varname_root}} STREQUAL "")
  VAL_NORMALIZE(local_opt ABS)

  # Initialize some variables
  set(TP_DISABLED 0)
  set(TP_DISABLE_TEST 0)
  set(TP_MET_CONDITION 0)

  # 0.  Whether or not we're building the sources, we are tracking the files
  # that are supposed to be in the directory
  get_filename_component(DIR_NAME "${dir}" NAME)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist")
    message(FATAL_ERROR "Third party component ${DIR_NAME} does not have a dist file at \"${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist\"")
  endif(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist")
  include("${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}.dist")
  CMAKEFILES_IN_DIR(${DIR_NAME}_ignore_files ${dir})

  # 1.  For executables, it is a reasonable use case that the developer manually specifies
  # the location for an executable.  It is tricky to distinguish this situation from
  # a previously cached executable path resulting from a Find*.cmake script.  The way
  # we will proceed is to cache the value of ${varname_root}_EXECUTABLE if it is defined, and
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
  if(${varname_root}_EXECUTABLE)
    IS_SUBPATH("${CMAKE_BINARY_DIR}" "${${varname_root}_EXECUTABLE}" SUBBUILD_TEST)
    if("${SUBBUILD_TEST}" STREQUAL "0")
      get_filename_component(FULL_PATH_EXEC ${${varname_root}_EXECUTABLE} ABSOLUTE)
      if("${FULL_PATH_EXEC}" STREQUAL "${${varname_root}_EXECUTABLE}")
	if(EXISTS ${FULL_PATH_EXEC})
	  set(EXEC_CACHED ${${varname_root}_EXECUTABLE})
	else(EXISTS ${FULL_PATH_EXEC})
	  # This path not being present may indicate the user specified a path
	  # and made a mistake doing so - warn that this might be the case.
	  message(WARNING "File path ${${varname_root}_EXECUTABLE} specified for ${varname_root}_EXECUTABLE does not exist - this path will not override ${build_target} executable search results.")
	endif(EXISTS ${FULL_PATH_EXEC})
      endif("${FULL_PATH_EXEC}" STREQUAL "${${varname_root}_EXECUTABLE}")
    endif("${SUBBUILD_TEST}" STREQUAL "0")
  endif(${varname_root}_EXECUTABLE)

  # 2. If any of the required flags are off, this tool is a no-go.
  set(DISABLE_STR "")
  foreach(item ${TP_REQUIRED_VARS})
    if(NOT ${item})
      set(TP_DISABLED 1)
      set(TP_DISABLE_TEST 1)
      set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
      if(NOT DISABLE_STR)
	set(DISABLE_STR "${item}")
      else(NOT DISABLE_STR)
	set(DISABLE_STR "${DISABLE_STR},${item}")
      endif(NOT DISABLE_STR)
    endif(NOT ${item})
  endforeach(item ${TP_REQUIRED_VARS})
  if(DISABLE_STR)
    set(${CMAKE_PROJECT_NAME}_${varname_root} "DISABLED ${DISABLE_STR}" CACHE STRING "DISABLED ${DISABLED_STR}" FORCE)
    mark_as_advanced(FORCE ${CMAKE_PROJECT_NAME}_${varname_root})
    set(${varname_root}_MET_CONDITION 1)
  else(DISABLE_STR)
    # If we have a leftover disabled setting in the cache from earlier runs, clear it.
    if("${local_opt}" MATCHES "DISABLED")
      set(${CMAKE_PROJECT_NAME}_${varname_root} "" CACHE STRING "Clear DISABLED setting" FORCE)
      mark_as_advanced(CLEAR ${CMAKE_PROJECT_NAME}_${varname_root})
    endif("${local_opt}" MATCHES "DISABLED")
  endif(DISABLE_STR)

  # 3. Next - is the executable variable explicitly set to SYSTEM?  If it is, we are NOT building it.
  if(local_opt)
    if("${local_opt}" STREQUAL "SYSTEM")
      set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
      set(TP_MET_CONDITION 2)
      if(TP_NOSYS)
	message(WARNING "Compilation of ${build_target} was disabled, but local copy is modified - using a system version of ${build_target} may introduce problems or even fail to work!")
      endif(TP_NOSYS)
    endif("${local_opt}" STREQUAL "SYSTEM")
  endif(local_opt)

  # 4. If we have a NOSYS flag, ALWAYS* use the bundled version.  The NOSYS flag signifies that
  # the BRL-CAD project requires modifications in the local src/other version of a library or
  # tool that are not likely to be present in a system version.  These flags should be periodically
  # reviewed to determine if they can be removed (i.e. system packages have appeared in modern
  # OS distributions with the fixes needed by BRL-CAD...).
  #
  # * In the case of executables, we'll allow a cached value from the user to override the NOSYS
  # flag, since that's a likely scenario for testing, but that shouldn't be done except for testing
  # purposes with a NOSYS target.
  if(NOT TP_MET_CONDITION AND TP_NOSYS)
    set(${CMAKE_PROJECT_NAME}_${varname_root} "BUNDLED" CACHE STRING "NOSYS passed, using bundled ${build_target}" FORCE)
    mark_as_advanced(${CMAKE_PROJECT_NAME}_${varname_root})
    set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
    set(TP_MET_CONDITION 3)
    set(TP_DISABLE_TEST 1)
  endif(NOT TP_MET_CONDITION AND TP_NOSYS)

  # 5. If we have an explicit BUNDLE request for this particular executable,  honor it as long as
  # features are satisfied.  No point in testing if we know we're turning it on - set vars accordingly.
  if(NOT TP_MET_CONDITION AND "${local_opt}" STREQUAL "BUNDLED")
    set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
    set(TP_DISABLE_TEST 1)
    set(TP_MET_CONDITION 4)
  endif(NOT TP_MET_CONDITION AND "${local_opt}" STREQUAL "BUNDLED")


  # 5. If BRLCAD_BUNDLED_LIBS is exactly SYSTEM or exactly BUNDLED, and we haven't been overridden by
  # one of the other conditions above, go with that.
  if(NOT TP_MET_CONDITION)
    if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
      set(${CMAKE_PROJECT_NAME}_${varname_root} "${BRLCAD_BUNDLED_LIBS} (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS: ${BRLCAD_BUNDLED_LIBS}" FORCE)
      if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
	set(TP_MET_CONDITION 5)
      elseif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(TP_DISABLE_TEST 1)
	set(TP_MET_CONDITION 5)
      endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
    endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
  endif(NOT TP_MET_CONDITION)

  # If we haven't been knocked out by any of the above conditions, do our testing and base the results on that.
  if(NOT TP_DISABLE_TEST)
    # Stash the previous results (if any) so we don't repeatedly call out the tests - only report
    # if something actually changes in subsequent runs.
    set(${varname_root}_FOUND_STATUS ${${varname_root}_FOUND})

    # Initialize (or rather, uninitialize) variables in preparation for search
    set(${varname_root}_FOUND "${varname_root}-NOTFOUND" CACHE STRING "${varname_root}_FOUND" FORCE)
    mark_as_advanced(${varname_root}_FOUND)
    set(${varname_root}_EXECUTABLE "${varname_root}-NOTFOUND" CACHE STRING "${varname_root}_EXECUTABLE" FORCE)

    # Be quiet if we're doing this over
    if("${${varname_root}_FOUND_STATUS}" MATCHES "NOTFOUND")
      set(${varname_root}_FIND_QUIETLY TRUE)
    endif("${${varname_root}_FOUND_STATUS}" MATCHES "NOTFOUND")

    # Include the Find module for the executable in question
    if(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${varname_root}.cmake)
      include(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${varname_root}.cmake)
    else(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${varname_root}.cmake)
      include(${CMAKE_ROOT}/Modules/Find${varname_root}.cmake)
    endif(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${varname_root}.cmake)

    # going to use system or bundled versions of deps
    if(${varname_root}_FOUND)
      set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
      set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
    else(${varname_root}_FOUND)
      # If one of our previous conditions precludes building this library, we've got a problem unless the
      # cached version is suitable - check that before we warn.
      if(NOT "${local_opt}" STREQUAL "SYSTEM" AND NOT "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${varname_root} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
      endif(NOT "${local_opt}" STREQUAL "SYSTEM" AND NOT "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
    endif(${varname_root}_FOUND)

  endif(NOT TP_DISABLE_TEST)

  # Now that we've run the Find routine, see if we had a cached value different from any of our
  # standard results
  set(TP_USING_CACHED 0)
  if(NOT "${TP_MET_CONDITION}" STREQUAL "1" AND NOT "${TP_MET_CONDITION}" STREQUAL "4")
    if(EXEC_CACHED)
      # Is it a build target?  If so, don't cache it.
      get_filename_component(EXEC_CACHED_ABS_PATH ${EXEC_CACHED} ABSOLUTE)
      IF ("${EXEC_CACHED_ABS_PATH}" STREQUAL "${PATH_ABS}")
	# Is it the bundled path? (don't override if it is, the bundled option setting takes care of that)
	if(NOT "${EXEC_CACHED}" STREQUAL "${CMAKE_BINARY_DIR}/${BIN_DIR}/${build_target}")
	  # Is it the same as the found results?
	  if(NOT "${EXEC_CACHED}" STREQUAL "${${varname_root}_EXECUTABLE_FOUND_RESULT}")
	    set(${varname_root}_EXECUTABLE ${EXEC_CACHED} CACHE STRING "Apparently a user specified path was supplied, use it" FORCE)
	    set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
	    set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM (AUTO)" CACHE STRING "Apparently a user specified path was supplied, use it" FORCE)
	    set(TP_USING_CACHED 1)
	  endif(NOT "${EXEC_CACHED}" STREQUAL "${${varname_root}_EXECUTABLE_FOUND_RESULT}")
	endif(NOT "${EXEC_CACHED}" STREQUAL "${CMAKE_BINARY_DIR}/${BIN_DIR}/${build_target}")
      endif("${EXEC_CACHED_ABS_PATH}" STREQUAL "${PATH_ABS}")
    endif(EXEC_CACHED)
  endif(NOT "${TP_MET_CONDITION}" STREQUAL "1" AND NOT "${TP_MET_CONDITION}" STREQUAL "4")

  # If the CACHED value doesn't look good, and we aren't overridden by any of the other conditions, set based
  # on the test results
  if(NOT TP_USING_CACHED AND NOT TP_DISABLE_TEST)
    if(${varname_root}_FOUND)
      set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD OFF)
      if(NOT "${CMAKE_PROJECT_NAME}_${varname_root}" STREQUAL "SYSTEM")
	set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
      endif(NOT "${CMAKE_PROJECT_NAME}_${varname_root}" STREQUAL "SYSTEM")
    else(${varname_root}_FOUND)
      # If one of our previous conditions precludes building this exec, we've got a problem.
      if("${local_opt}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_NOTFOUND 1)
	message(WARNING "Compilation of local version of ${build_target} was disabled, but system version not found!")
	if(NOT "${local_opt}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS forced to SYSTEM, but library not found!" FORCE)
	else(NOT "${local_opt}" STREQUAL "SYSTEM")
	  set(${CMAKE_PROJECT_NAME}_${varname_root} "SYSTEM" CACHE STRING "Hard-set to SYSTEM by user, but library not found!" FORCE)
	endif(NOT "${local_opt}" STREQUAL "SYSTEM")
      else("${local_opt}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	set(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${varname_root} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
      endif("${local_opt}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
    endif(${varname_root}_FOUND)
  endif(NOT TP_USING_CACHED AND NOT TP_DISABLE_TEST)


  if(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)
    # In the case of executables, it is possible that a directory may define more than one
    # executable target.  If this is the case, we may have already added this directory -
    # if so, don't do it again.
    if(MISC_ADDED_DIRS)
      list(FIND MISC_ADDED_DIRS ${dir} ADDED_RESULT)
      if("${ADDED_RESULT}" STREQUAL "-1")
	add_subdirectory(${dir})
	set(MISC_ADDED_DIRS ${MISC_ADDED_DIRS} ${dir})
	list(REMOVE_DUPLICATES MISC_ADDED_DIRS)
	set(MISC_ADDED_DIRS ${MISC_ADDED_DIRS} CACHE STRING "Enabled 3rd party sub-directories" FORCE)
	mark_as_advanced(MISC_ADDED_DIRS)
      endif("${ADDED_RESULT}" STREQUAL "-1")
    else(MISC_ADDED_DIRS)
      add_subdirectory(${dir})
      set(MISC_ADDED_DIRS ${dir} CACHE STRING "Enabled 3rd party sub-directories" FORCE)
      mark_as_advanced(MISC_ADDED_DIRS)
    endif(MISC_ADDED_DIRS)
    if(CMAKE_CONFIGURATION_TYPES)
      set(${varname_root}_EXECUTABLE "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${BIN_DIR}/${build_target}" CACHE STRING "${varname_root}_EXECUTABLE" FORCE)
    else(CMAKE_CONFIGURATION_TYPES)
      set(${varname_root}_EXECUTABLE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${build_target}" CACHE STRING "${varname_root}_EXECUTABLE" FORCE)
    endif(CMAKE_CONFIGURATION_TYPES)
    set(${varname_root}_EXECUTABLE_TARGET ${build_target} CACHE STRING "Build target for ${build_target}" FORCE)
  else(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)
    set(${varname_root}_EXECUTABLE_TARGET "" CACHE STRING "No build target for ${build_target}" FORCE)
  endif(${CMAKE_PROJECT_NAME}_${varname_root}_BUILD)

  if (TP_ALIASES AND TP_DESCRIPTION)
    BRLCAD_OPTION("${CMAKE_PROJECT_NAME}_${varname_root}" "${${CMAKE_PROJECT_NAME}_${varname_root}}"
      TYPE ABS
      ALIASES ${TP_ALIASES}
      DESCRIPTION "${TP_DESCRIPTION}")
  endif (TP_ALIASES AND TP_DESCRIPTION)

  mark_as_advanced(${varname_root}_EXECUTABLE)
  mark_as_advanced(${varname_root}_EXECUTABLE_TARGET)
  mark_as_advanced(${varname_root}_FOUND)

endfunction(THIRD_PARTY_EXECUTABLE)

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
