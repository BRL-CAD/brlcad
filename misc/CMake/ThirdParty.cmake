#                T H I R D P A R T Y . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY lower dir required_vars aliases description)
    STRING(TOUPPER ${lower} upper)
    # If the library variable has been explicitly set, get
    # an uppercase version of it for easier matching
    IF(${CMAKE_PROJECT_NAME}_${upper})
	STRING(TOUPPER ${${CMAKE_PROJECT_NAME}_${upper}} OPT_STR_UPPER)
    ELSE(${CMAKE_PROJECT_NAME}_${upper})
	SET(OPT_STR_UPPER "")
    ENDIF(${CMAKE_PROJECT_NAME}_${upper})
    IF(OPT_STR_UPPER)
	IF(${OPT_STR_UPPER} STREQUAL "ON")
	    SET(OPT_STR_UPPER "BUNDLED")
	ENDIF(${OPT_STR_UPPER} STREQUAL "ON")
	IF(${OPT_STR_UPPER} STREQUAL "OFF")
	    SET(OPT_STR_UPPER "SYSTEM")
	ENDIF(${OPT_STR_UPPER} STREQUAL "OFF")
    ENDIF(OPT_STR_UPPER)

    # Initialize some variables
    SET(${upper}_DISABLED 0)
    SET(${upper}_DISABLE_TEST 0)
    SET(${upper}_MET_CONDITION 0)

    # 1. If any of the required flags are off, this extension is a no-go.
    SET(DISABLE_STR "") 
    FOREACH(item ${required_vars})
	IF(NOT ${item})
	    SET(${upper}_DISABLED 1)
	    SET(${upper}_DISABLE_TEST 1)
	    SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    IF(NOT DISABLE_STR) 
		SET(DISABLE_STR "${item}")
	    ELSE(NOT DISABLE_STR) 
		SET(DISABLE_STR "${DISABLE_STR},${item}")
	    ENDIF(NOT DISABLE_STR) 
	ENDIF(NOT ${item})
    ENDFOREACH(item ${required_vars})
    IF(DISABLE_STR)
	SET(${CMAKE_PROJECT_NAME}_${upper} "DISABLED ${DISABLE_STR}" CACHE STRING "DISABLED ${DISABLED_STR}" FORCE)
	MARK_AS_ADVANCED(FORCE ${CMAKE_PROJECT_NAME}_${upper})
	SET(${upper}_MET_CONDITION 1)
    ELSE(DISABLE_STR)
	# If we have a leftover disabled setting in the cache from earlier runs, clear it.
	IF("${CMAKE_PROJECT_NAME}_${upper}" MATCHES "DISABLED")
	    SET(${CMAKE_PROJECT_NAME}_${upper} "" CACHE STRING "Clear DISABLED setting" FORCE)
	    MARK_AS_ADVANCED(CLEAR ${CMAKE_PROJECT_NAME}_${upper})
	ENDIF("${CMAKE_PROJECT_NAME}_${upper}" MATCHES "DISABLED")
    ENDIF(DISABLE_STR)


    # 2. If we have a NOSYS flag, ALWAYS use the bundled version.  The NOSYS flag signifies that
    # the BRL-CAD project requires modifications in the local src/other version of a library or
    # tool that are not likely to be present in a system version.  These flags should be periodically
    # reviewed to determine if they can be removed (i.e. system packages have appeared in modern
    # OS distributions with the fixes needed by BRL-CAD...)
    IF(NOT ${upper}_MET_CONDITION)
	FOREACH(extraarg ${ARGN})
	    IF(extraarg STREQUAL "NOSYS")
		SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED" CACHE STRING "NOSYS passed, using bundled ${lower}" FORCE)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_MET_CONDITION 2)
		SET(${upper}_DISABLE_TEST 1)
	    ENDIF(extraarg STREQUAL "NOSYS")
	ENDFOREACH(extraarg ${ARGN})
    ENDIF(NOT ${upper}_MET_CONDITION)


    # 3. Next - is the library variable explicitly set to SYSTEM?  If it is, we are NOT building it.
    IF(OPT_STR_UPPER)
	IF("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	    SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    SET(${upper}_MET_CONDITION 3)
	ENDIF("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
    ENDIF(OPT_STR_UPPER)

    # 4. If we have an explicit BUNDLE request for this particular library,  honor it as long as 
    # features are satisfied.  No point in testing if we know we're turning it on - set vars accordingly.
    IF(OPT_STR_UPPER)
	IF("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
	    IF(NOT ${upper}_MET_CONDITION)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_DISABLE_TEST 1)
		SET(${upper}_MET_CONDITION 4)
	    ENDIF(NOT ${upper}_MET_CONDITION)
	ENDIF("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
    ENDIF(OPT_STR_UPPER)


    # 5. If BRLCAD_BUNDLED_LIBS is exactly SYSTEM or exactly BUNDLED, and we haven't been overridden by
    # one of the other conditions above, go with that.
    IF(NOT ${upper}_MET_CONDITION)
	IF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
	    SET(${CMAKE_PROJECT_NAME}_${upper} "${BRLCAD_BUNDLED_LIBS} (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS: ${BRLCAD_BUNDLED_LIBS}" FORCE)
	    IF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
		SET(${upper}_MET_CONDITION 5)
	    ELSEIF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_DISABLE_TEST 1)
		SET(${upper}_MET_CONDITION 5)
	    ENDIF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
	ENDIF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
    ENDIF(NOT ${upper}_MET_CONDITION)


    # If we haven't been knocked out by any of the above conditions, do our testing and base the results on that.

    IF(NOT ${upper}_DISABLE_TEST)
	# Stash the previous results (if any) so we don't repeatedly call out the tests - only report
	# if something actually changes in subsequent runs.
	SET(${upper}_FOUND_STATUS ${${upper}_FOUND})

	# Initialize (or rather, uninitialize) variables in preparation for search
	SET(${upper}_FOUND "${upper}-NOTFOUND" CACHE STRING "${upper}_FOUND" FORCE)
	MARK_AS_ADVANCED(${upper}_FOUND)
	SET(${upper}_LIBRARY "${upper}-NOTFOUND" CACHE STRING "${upper}_LIBRARY" FORCE)
	SET(${upper}_INCLUDE_DIR "${upper}-NOTFOUND" CACHE STRING "${upper}_INCLUDE_DIR" FORCE)

	# Be quiet if we're doing this over
	IF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")
	    SET(${upper}_FIND_QUIETLY TRUE)
	ENDIF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")

	# Include the Find module for the library in question
	IF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
	    INCLUDE(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
	ELSE(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
	    INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
	ENDIF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)

	# going to use system or bundled versions of deps
	IF(${upper}_FOUND)
	    SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
	ELSE(${upper}_FOUND)
	    # If one of our previous conditions precludes building this library, we've got a problem.
	    IF("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
		SET(${CMAKE_PROJECT_NAME}_${upper}_NOTFOUND 1)
		MESSAGE(WARNING "Compilation of local version of ${lower} was disabled, but system version not found!")
		IF(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
		    SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS forced to SYSTEM, but library not found!" FORCE)
		ELSE(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
		    SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM" CACHE STRING "Hard-set to SYSTEM by user, but library not found!" FORCE)
		ENDIF(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	    ELSE("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
	    ENDIF("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	ENDIF(${upper}_FOUND)
    ENDIF(NOT ${upper}_DISABLE_TEST)

    # If we're going with a system version of a library, we have to remove any previously built 
    # output from enabled bundled copies of the library in question, or the linker will get 
    # confused - a system lib was found and system libraries are to be preferred with current 
    # options.  This is unfortunate in that it may introduce extra build work just from trying 
    # configure options, but appears to be essential to ensuring that the build "just works" 
    # each time.
    IF(NOT ${CMAKE_PROJECT_NAME}_${upper}_BUILD)
	STRING(REGEX REPLACE "lib" "" rootname "${lower}")
	FILE(GLOB STALE_FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${rootname}*${CMAKE_SHARED_LIBRARY_SUFFIX}*")

	FOREACH(stale_file ${STALE_FILES})
	    EXEC_PROGRAM(
		${CMAKE_COMMAND} ARGS -E remove ${stale_file}
		OUTPUT_VARIABLE rm_out
		RETURN_VALUE rm_retval
		)
	ENDFOREACH(stale_file ${STALE_FILES})

	IF(NOT "${CMAKE_PROJECT_NAME}_${upper}" MATCHES "${OPT_STR_UPPER}")
	    IF("${OPT_STR_UPPER}" MATCHES "BUNDLED")
		MESSAGE("Reconfiguring to use system ${upper}")
	    ENDIF("${OPT_STR_UPPER}" MATCHES "BUNDLED")
	ENDIF(NOT "${CMAKE_PROJECT_NAME}_${upper}" MATCHES "${OPT_STR_UPPER}")
    ENDIF(NOT ${CMAKE_PROJECT_NAME}_${upper}_BUILD)

    # After all that, we know now what to do about src/other/${dir} - do it
    IF(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
	ADD_SUBDIRECTORY(${dir})
	SET(${upper}_LIBRARY "${lower}" CACHE STRING "${upper}_LIBRARY" FORCE)
	SET(${upper}_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${dir};${CMAKE_CURRENT_BINARY_DIR}/${dir} CACHE STRING "set by THIRD_PARTY_SUBDIR macro" FORCE)
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	    DISTCHECK_IGNORE(${dir} ${dir}_ignore_files)
	ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	    MESSAGE("Bundled build, but file ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist not found")
	ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
    ELSE(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
	DISTCHECK_IGNORE_ITEM(${dir})
    ENDIF(${CMAKE_PROJECT_NAME}_${upper}_BUILD)

    OPTION_ALIASES("${CMAKE_PROJECT_NAME}_${upper}" "${aliases}")
    OPTION_DESCRIPTION("${CMAKE_PROJECT_NAME}_${upper}" "${aliases}" "${description}")

    MARK_AS_ADVANCED(${upper}_LIBRARY)
    MARK_AS_ADVANCED(${upper}_INCLUDE_DIR)
ENDMACRO(THIRD_PARTY)


#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_EXECUTABLE lower dir required_vars aliases description)
    STRING(TOUPPER ${lower} upper)
    # If the exec variable has been explicitly set, get
    # an uppercase version of it for easier matching
    IF(${CMAKE_PROJECT_NAME}_${upper})
	STRING(TOUPPER ${${CMAKE_PROJECT_NAME}_${upper}} OPT_STR_UPPER)
    ELSE(${CMAKE_PROJECT_NAME}_${upper})
	SET(OPT_STR_UPPER "")
    ENDIF(${CMAKE_PROJECT_NAME}_${upper})
    IF(OPT_STR_UPPER)
	IF(${OPT_STR_UPPER} STREQUAL "ON")
	    SET(OPT_STR_UPPER "BUNDLED")
	ENDIF(${OPT_STR_UPPER} STREQUAL "ON")
	IF(${OPT_STR_UPPER} STREQUAL "OFF")
	    SET(OPT_STR_UPPER "SYSTEM")
	ENDIF(${OPT_STR_UPPER} STREQUAL "OFF")
    ENDIF(OPT_STR_UPPER)

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
    IF(${upper}_EXECUTABLE)
	IF(NOT "${${upper}_EXECUTABLE}" MATCHES ${CMAKE_BINARY_DIR})
	    get_filename_component(FULL_PATH_EXEC ${${upper}_EXECUTABLE} ABSOLUTE)
	    IF ("${FULL_PATH_EXEC}" STREQUAL "${${upper}_EXECUTABLE}")
		IF(EXISTS ${FULL_PATH_EXEC})
		    SET(EXEC_CACHED ${${upper}_EXECUTABLE})
		ELSE(EXISTS ${FULL_PATH_EXEC})
		    # This path not being present may indicate the user specified a path 
		    # and made a mistake doing so - warn that this might be the case.
		    MESSAGE(WARNING "File path ${${upper}_EXECUTABLE} specified for ${upper}_EXECUTABLE does not exist - this path will not override ${lower} executable search results.")
		ENDIF(EXISTS ${FULL_PATH_EXEC})
	    ENDIF ("${FULL_PATH_EXEC}" STREQUAL "${${upper}_EXECUTABLE}")
	ENDIF(NOT "${${upper}_EXECUTABLE}" MATCHES ${CMAKE_BINARY_DIR})
    ENDIF(${upper}_EXECUTABLE)
 
    # Initialize some variables
    SET(${upper}_DISABLED 0)
    SET(${upper}_DISABLE_TEST 0)
    SET(${upper}_MET_CONDITION 0)

    # 1. If any of the required flags are off, this tool is a no-go.
    SET(DISABLE_STR "") 
    FOREACH(item ${required_vars})
	IF(NOT ${item})
	    SET(${upper}_DISABLED 1)
	    SET(${upper}_DISABLE_TEST 1)
	    SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    IF(NOT DISABLE_STR) 
		SET(DISABLE_STR "${item}")
	    ELSE(NOT DISABLE_STR) 
		SET(DISABLE_STR "${DISABLE_STR},${item}")
	    ENDIF(NOT DISABLE_STR) 
	ENDIF(NOT ${item})
    ENDFOREACH(item ${required_vars})
    IF(DISABLE_STR)
	SET(${CMAKE_PROJECT_NAME}_${upper} "DISABLED ${DISABLE_STR}" CACHE STRING "DISABLED ${DISABLED_STR}" FORCE)
	MARK_AS_ADVANCED(FORCE ${CMAKE_PROJECT_NAME}_${upper})
	SET(${upper}_MET_CONDITION 1)
    ELSE(DISABLE_STR)
	# If we have a leftover disabled setting in the cache from earlier runs, clear it.
	IF("${CMAKE_PROJECT_NAME}_${upper}" MATCHES "DISABLED")
	    SET(${CMAKE_PROJECT_NAME}_${upper} "" CACHE STRING "Clear DISABLED setting" FORCE)
	    MARK_AS_ADVANCED(CLEAR ${CMAKE_PROJECT_NAME}_${upper})
	ENDIF("${CMAKE_PROJECT_NAME}_${upper}" MATCHES "DISABLED")
    ENDIF(DISABLE_STR)

    # 2. If we have a NOSYS flag, ALWAYS* use the bundled version.  The NOSYS flag signifies that
    # the BRL-CAD project requires modifications in the local src/other version of a library or
    # tool that are not likely to be present in a system version.  These flags should be periodically
    # reviewed to determine if they can be removed (i.e. system packages have appeared in modern
    # OS distributions with the fixes needed by BRL-CAD...).  
    # 
    # * In the case of executables, we'll allow a cached value from the user to override the NOSYS 
    # flag, since that's a likely scenario for testing, but that shouldn't be done except for testing
    # purposes with a NOSYS target.
    IF(NOT ${upper}_MET_CONDITION)
	FOREACH(extraarg ${ARGN})
	    IF(extraarg STREQUAL "NOSYS")
		SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED" CACHE STRING "NOSYS passed, using bundled ${lower}" FORCE)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_MET_CONDITION 2)
		SET(${upper}_DISABLE_TEST 1)
	    ENDIF(extraarg STREQUAL "NOSYS")
	ENDFOREACH(extraarg ${ARGN})
    ENDIF(NOT ${upper}_MET_CONDITION)


    # 3. Next - is the executable variable explicitly set to SYSTEM?  If it is, we are NOT building it.
    IF(OPT_STR_UPPER)
	IF("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	    SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    SET(${upper}_MET_CONDITION 3)
	ENDIF("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
    ENDIF(OPT_STR_UPPER)

    # 4. If we have an explicit BUNDLE request for this particular executable,  honor it as long as 
    # features are satisfied.  No point in testing if we know we're turning it on - set vars accordingly.
    IF(OPT_STR_UPPER)
	IF("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
	    IF(NOT ${upper}_MET_CONDITION)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_DISABLE_TEST 1)
		SET(${upper}_MET_CONDITION 4)
	    ENDIF(NOT ${upper}_MET_CONDITION)
	ENDIF("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
    ENDIF(OPT_STR_UPPER)


    # 5. If BRLCAD_BUNDLED_LIBS is exactly SYSTEM or exactly BUNDLED, and we haven't been overridden by
    # one of the other conditions above, go with that.
    IF(NOT ${upper}_MET_CONDITION)
	IF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
	    SET(${CMAKE_PROJECT_NAME}_${upper} "${BRLCAD_BUNDLED_LIBS} (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS: ${BRLCAD_BUNDLED_LIBS}" FORCE)
	    IF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
		SET(${upper}_MET_CONDITION 5)
	    ELSEIF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_DISABLE_TEST 1)
		SET(${upper}_MET_CONDITION 5)
	    ENDIF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM")
	ENDIF("${BRLCAD_BUNDLED_LIBS}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED")
    ENDIF(NOT ${upper}_MET_CONDITION)

    # If we haven't been knocked out by any of the above conditions, do our testing and base the results on that.
    IF(NOT ${upper}_DISABLE_TEST)
	# Stash the previous results (if any) so we don't repeatedly call out the tests - only report
	# if something actually changes in subsequent runs.
	SET(${upper}_FOUND_STATUS ${${upper}_FOUND})

	# Initialize (or rather, uninitialize) variables in preparation for search
	SET(${upper}_FOUND "${upper}-NOTFOUND" CACHE STRING "${upper}_FOUND" FORCE)
	MARK_AS_ADVANCED(${upper}_FOUND)
	SET(${upper}_EXECUTABLE "${upper}-NOTFOUND" CACHE STRING "${upper}_EXECUTABLE" FORCE)

	# Be quiet if we're doing this over
	IF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")
	    SET(${upper}_FIND_QUIETLY TRUE)
	ENDIF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")

	# Include the Find module for the library in question
	IF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
	    INCLUDE(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
	ELSE(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
	    INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
	ENDIF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)

	# going to use system or bundled versions of deps
	IF(${upper}_FOUND)
	    SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
	ELSE(${upper}_FOUND)
	    # If one of our previous conditions precludes building this library, we've got a problem unless the
	    # cached version is suitable - check that before we warn.
	    IF(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM" AND NOT "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
	    ENDIF(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM" AND NOT "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	ENDIF(${upper}_FOUND)
    ENDIF(NOT ${upper}_DISABLE_TEST)


    # Now that we've run the Find routine, see if we had a cached value different from any of our
    # standard results
    SET(${upper}_USING_CACHED 0)
    IF(NOT "${upper}_MET_CONDITION" STREQUAL "1" AND NOT "${upper}_MET_CONDITION" STREQUAL "4")
	IF(EXEC_CACHED)
	    # Is it a build target?  If so, don't cache it.
	    GET_FILENAME_COMPONENT(EXEC_CACHED_ABS_PATH ${EXEC_CACHED} ABSOLUTE)
	    IF ("${EXEC_CACHED_ABS_PATH}" STREQUAL "${PATH_ABS}")
		# Is it the bundled path? (don't override if it is, the bundled option setting takes care of that)
		IF(NOT "${EXEC_CACHED}" STREQUAL "${CMAKE_BINARY_DIR}/${BIN_DIR}/${lower}")
		    # Is it the same as the found results?
		    IF(NOT "${EXEC_CACHED}" STREQUAL "${${upper}_EXECUTABLE_FOUND_RESULT}")
			SET(${upper}_EXECUTABLE ${EXEC_CACHED} CACHE STRING "Apparently a user specified path was supplied, use it" FORCE)
			SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
			SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Apparently a user specified path was supplied, use it" FORCE)
			SET(${upper}_USING_CACHED 1)
		    ENDIF(NOT "${EXEC_CACHED}" STREQUAL "${${upper}_EXECUTABLE_FOUND_RESULT}")
		ENDIF(NOT "${EXEC_CACHED}" STREQUAL "${CMAKE_BINARY_DIR}/${BIN_DIR}/${lower}")
	    ENDIF("${EXEC_CACHED_ABS_PATH}" STREQUAL "${PATH_ABS}")
	ENDIF(EXEC_CACHED)
    ENDIF(NOT "${upper}_MET_CONDITION" STREQUAL "1" AND NOT "${upper}_MET_CONDITION" STREQUAL "4")

    # If the CACHED value doesn't look good, and we aren't overridden by any of the other conditions, set based
    # on the test results
    IF(NOT ${upper}_USING_CACHED AND NOT ${upper}_DISABLE_TEST)
	IF(${upper}_FOUND)
	    SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
	    SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Found System version, using" FORCE)
	ELSE(${upper}_FOUND)
	    # If one of our previous conditions precludes building this exec, we've got a problem.
	    IF("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${CMAKE_PROJECT_NAME}_${upper}_NOTFOUND 1)
		MESSAGE(WARNING "Compilation of local version of ${lower} was disabled, but system version not found!")
		IF(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
		    SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "BRLCAD_BUNDLED_LIBS forced to SYSTEM, but library not found!" FORCE)
		ELSE(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
		    SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM" CACHE STRING "Hard-set to SYSTEM by user, but library not found!" FORCE)
		ENDIF(NOT "${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	    ELSE("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "System test failed, enabling local copy" FORCE)
	    ENDIF("${OPT_STR_UPPER}" STREQUAL "SYSTEM" OR "${BRLCAD_BUNDLED_LIBS}" STREQUAL SYSTEM)
	ENDIF(${upper}_FOUND)
    ENDIF(NOT ${upper}_USING_CACHED AND NOT ${upper}_DISABLE_TEST)


    IF(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
	# In the case of executables, it is possible that a directory may define more than one
	# executable target.  If this is the case, we may have already added this directory - 
	# if so, don't do it again.
	IF(SRC_OTHER_ADDED_DIRS)
	    LIST(FIND SRC_OTHER_ADDED_DIRS ${dir} ADDED_RESULT)
	    IF("${ADDED_RESULT}" STREQUAL "-1")
		ADD_SUBDIRECTORY(${dir})
		SET(SRC_OTHER_ADDED_DIRS ${SRC_OTHER_ADDED_DIRS} ${dir})
		LIST(REMOVE_DUPLICATES SRC_OTHER_ADDED_DIRS)
		SET(SRC_OTHER_ADDED_DIRS ${SRC_OTHER_ADDED_DIRS} CACHE STRING "Enabled 3rd party sub-directories" FORCE)
		MARK_AS_ADVANCED(SRC_OTHER_ADDED_DIRS)
		IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
		    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
		    DISTCHECK_IGNORE(${dir} ${dir}_ignore_files)
		ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
		    MESSAGE("Bundled build, but file ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist not found")
		ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	    ENDIF("${ADDED_RESULT}" STREQUAL "-1")
	    SET(${upper}_EXECUTABLE "${lower}" CACHE STRING "${upper}_EXECUTABLE" FORCE)
	ELSE(SRC_OTHER_ADDED_DIRS)
	    ADD_SUBDIRECTORY(${dir})
	    SET(${upper}_EXECUTABLE "${lower}" CACHE STRING "${upper}_EXECUTABLE" FORCE)
	    SET(SRC_OTHER_ADDED_DIRS ${dir} CACHE STRING "Enabled 3rd party sub-directories" FORCE)
	    MARK_AS_ADVANCED(SRC_OTHER_ADDED_DIRS)
	    IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
		INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
		DISTCHECK_IGNORE(${dir} ${dir}_ignore_files)
	    ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
		MESSAGE("Bundled build, but file ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist not found")
	    ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	ENDIF(SRC_OTHER_ADDED_DIRS)
	SET(${upper}_EXECUTABLE_TARGET ${lower} CACHE STRING "Build target for ${lower}" FORCE)
    ELSE(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
	DISTCHECK_IGNORE_ITEM(${dir})
	SET(${upper}_EXECUTABLE_TARGET "" CACHE STRING "No build target for ${lower}" FORCE)
    ENDIF(${CMAKE_PROJECT_NAME}_${upper}_BUILD)

    OPTION_ALIASES("${CMAKE_PROJECT_NAME}_${upper}" "${aliases}")
    OPTION_DESCRIPTION("${CMAKE_PROJECT_NAME}_${upper}" "${aliases}" "${description}")

    MARK_AS_ADVANCED(${upper}_EXECUTABLE)
    MARK_AS_ADVANCED(${upper}_EXECUTABLE_TARGET)
    MARK_AS_ADVANCED(${upper}_FOUND)
ENDMACRO(THIRD_PARTY_EXECUTABLE)

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
