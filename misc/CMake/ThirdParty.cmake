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
MACRO(THIRD_PARTY lower dir)
	STRING(TOUPPER ${lower} upper)
	BUNDLE_OPTION(${CMAKE_PROJECT_NAME}_BUNDLED_LIBS ${CMAKE_PROJECT_NAME}_BUNDLED_LIBS ${CMAKE_PROJECT_NAME}_${upper} "")

	FOREACH(extraarg ${ARGN})
		IF(extraarg STREQUAL "NOSYS")
			IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "NOSYS passed, using bundled ${lower}" FORCE)
			ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
		ENDIF(extraarg STREQUAL "NOSYS")
	ENDFOREACH(extraarg ${ARGN})

	# Main search logic
	IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "BUNDLED")

		# BUNDLED or BUNDLED (AUTO), figure out which

		IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
			SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "Automatically using bundled" FORCE)
		ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
			SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED" CACHE STRING "Using bundled" FORCE)
		ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")

		# turn it on
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_LIBRARY "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)

	ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "BUNDLED")

		# AUTO or SYSTEM (AUTO) or SYSTEM, figure out which

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

		# handle conversion of *AUTO to indicate whether it's
		# going to use system or bundled versions of deps
		IF(${upper}_FOUND)

			# turn it off, found it
			SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
        		SET(${upper}_FOUND "TRUE" CACHE STRING "${upper}_FOUND" FORCE)

			# found system-installed 3rd-party dep
			IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Automatically using system, ${lower} found" FORCE)
			ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				# not auto, must be SYSTEM
				SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM" CACHE STRING "Using system, ${lower} found" FORCE)
			ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")

		ELSE(${upper}_FOUND)

			# turn it on, didn't find it
        		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
			SET(${upper}_LIBRARY "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)

			# did NOT find system-installed 3rd-party dep
                        IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "SYSTEM")
					SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Automatically attempting to use system even though ${lower} was NOT found" FORCE)
				        MESSAGE(WARNING "Configuring to use system ${lower} even though it was NOT found")

					# turn it off even though we didn't find it
					SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)

				ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "SYSTEM")
					SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "Automatically using bundled, ${lower} NOT found" FORCE)
				ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "SYSTEM")
                        ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				# not auto, must be SYSTEM
				MESSAGE(FATAL_ERROR "System version of ${lower} NOT found, halting due to setting")
                        ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")

		ENDIF(${upper}_FOUND)

		# We have to remove any previously built output from enabled bundled copies of the
		# library in question, or the linker will get confused - a system lib was found and
		# system libraries are to be preferred with current options.  This is unfortunate in
		# that it may introduce extra build work just from trying configure options, but appears
		# to be essential to ensuring that the build "just works" each time.
		IF(${upper}_FOUND)
			STRING(REGEX REPLACE "lib" "" rootname "${lower}")
			FILE(GLOB STALE_FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${rootname}*${CMAKE_SHARED_LIBRARY_SUFFIX}*")

			FOREACH(stale_file ${STALE_FILES})
				EXEC_PROGRAM(
					${CMAKE_COMMAND} ARGS -E remove ${stale_file}
					OUTPUT_VARIABLE rm_out
					RETURN_VALUE rm_retval
					)
			ENDFOREACH(stale_file ${STALE_FILES})

			IF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")
				MESSAGE("Reconfiguring to use system ${upper}")
			ENDIF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")
		ENDIF(${upper}_FOUND)
	ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "BUNDLED")

	IF(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
		ADD_SUBDIRECTORY(${dir})
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

	MARK_AS_ADVANCED(${upper}_LIBRARY)
	MARK_AS_ADVANCED(${upper}_INCLUDE_DIR)
ENDMACRO(THIRD_PARTY)


#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_EXECUTABLE lower dir)
	STRING(TOUPPER ${lower} upper)
	BUNDLE_OPTION(${CMAKE_PROJECT_NAME}_BUNDLED_LIBS ${CMAKE_PROJECT_NAME}_BUNDLED_LIBS ${CMAKE_PROJECT_NAME}_${upper} "")

	FOREACH(extraarg ${ARGN})
		IF(extraarg STREQUAL "NOSYS")
			IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "NOSYS passed, using bundled ${lower}" FORCE)
			ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
		ENDIF(extraarg STREQUAL "NOSYS")
	ENDFOREACH(extraarg ${ARGN})

	# Main search logic
	IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "BUNDLED")

		# BUNDLED or BUNDLED (AUTO), figure out which

		IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
			SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "Automatically using bundled" FORCE)
		ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
			SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED" CACHE STRING "Using bundled" FORCE)
		ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")

		# Include the Find module for the exec in question - need macro routines
		IF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
		ELSE(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
		ENDIF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)

		# turn it on
		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
		SET(${upper}_EXECUTABLE "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)

	ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "BUNDLED")

		# AUTO or SYSTEM (AUTO) or SYSTEM, figure out which

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

		# Include the Find module for the exec in question
		IF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
		ELSE(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
		ENDIF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)

		# handle conversion of *AUTO to indicate whether it's
		# going to use system or bundled versions of deps
		IF(${upper}_FOUND)

			# turn it off, found it
			SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD OFF)
        		SET(${upper}_FOUND "TRUE" CACHE STRING "${upper}_FOUND" FORCE)

			# found system-installed 3rd-party dep
			IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Automatically using system, ${lower} found" FORCE)
			ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				# not auto, must be SYSTEM
				SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM" CACHE STRING "Using system, ${lower} found" FORCE)
			ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")

		ELSE(${upper}_FOUND)

			# turn it on, didn't find it
        		SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)
			SET(${upper}_EXECUTABLE "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)

			# did NOT find system-installed 3rd-party dep
                        IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				IF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "SYSTEM")
					SET(${CMAKE_PROJECT_NAME}_${upper} "SYSTEM (AUTO)" CACHE STRING "Automatically attempting to use system even though ${lower} was NOT found" FORCE)
				        MESSAGE(WARNING "Configuring to use system ${lower} even though it was NOT found")

					# turn it off even though we didn't find it
					SET(${CMAKE_PROJECT_NAME}_${upper}_BUILD ON)

				ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "SYSTEM")
					SET(${CMAKE_PROJECT_NAME}_${upper} "BUNDLED (AUTO)" CACHE STRING "Automatically using bundled, ${lower} NOT found" FORCE)
				ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "SYSTEM")
                        ELSE(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")
				# not auto, must be SYSTEM
				MESSAGE(FATAL_ERROR "System version of ${lower} NOT found, halting due to setting")
                        ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "AUTO")

		ENDIF(${upper}_FOUND)

		IF(${upper}_FOUND)
			IF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")
				MESSAGE("Reconfiguring to use system ${upper}")
			ENDIF("${${upper}_FOUND_STATUS}" MATCHES "NOTFOUND")
		ENDIF(${upper}_FOUND)
	ENDIF(${CMAKE_PROJECT_NAME}_${upper} MATCHES "BUNDLED")

	IF(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
		ADD_SUBDIRECTORY(${dir})
		IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
			INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
			DISTCHECK_IGNORE(${dir} ${dir}_ignore_files)
		ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
			MESSAGE("Bundled build, but file ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist not found")
		ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}.dist)
	ELSE(${CMAKE_PROJECT_NAME}_${upper}_BUILD)
		DISTCHECK_IGNORE_ITEM(${dir})
	ENDIF(${CMAKE_PROJECT_NAME}_${upper}_BUILD)

	MARK_AS_ADVANCED(${upper}_EXECUTABLE)
ENDMACRO(THIRD_PARTY_EXECUTABLE)

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
