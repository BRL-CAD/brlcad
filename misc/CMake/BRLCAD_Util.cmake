#-----------------------------------------------------------------------------
# Pretty-printing macro that generates a box around a string and prints the
# resulting message.
MACRO(BOX_PRINT input_string border_string)
	STRING(LENGTH ${input_string} MESSAGE_LENGTH)
	STRING(LENGTH ${border_string} SEPARATOR_STRING_LENGTH)
	WHILE(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
		SET(SEPARATOR_STRING "${SEPARATOR_STRING}${border_string}")
		STRING(LENGTH ${SEPARATOR_STRING} SEPARATOR_STRING_LENGTH)
	ENDWHILE(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
	MESSAGE("${SEPARATOR_STRING}")
	MESSAGE("${input_string}")
	MESSAGE("${SEPARATOR_STRING}")
ENDMACRO()


#-----------------------------------------------------------------------------
# Macro for three-way options that optionally check whether a system
# resource is available.
MACRO(BUNDLE_OPTION optname default_raw)
	STRING(TOUPPER "${default_raw}" default)
	IF(NOT ${optname})
		IF(default)
			SET(${optname} "${default}" CACHE STRING "Using bundled ${optname}")
		ELSE(default)
			IF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "BUNDLED")
				SET(${optname} "BUNDLED (AUTO)" CACHE STRING "Using bundled ${optname}")
			ENDIF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "BUNDLED")

			IF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "SYSTEM")
				SET(${optname} "SYSTEM (AUTO)" CACHE STRING "Using system ${optname}")
			ENDIF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "SYSTEM")

			IF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "AUTO")
				SET(${optname} "AUTO" CACHE STRING "Using bundled ${optname}")
			ENDIF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "AUTO")
		ENDIF(default)
	ENDIF(NOT ${optname})

        # convert ON/OFF value to BUNDLED/SYSTEM
	STRING(TOUPPER "${${optname}}" optname_upper)
	IF(${optname_upper} MATCHES "ON")
		SET(optname_upper "BUNDLED")
	ENDIF(${optname_upper} MATCHES "ON")
	IF(${optname_upper} MATCHES "OFF")
		SET(optname_upper "SYSTEM")
	ENDIF(${optname_upper} MATCHES "OFF")
	SET(${optname} ${optname_upper})

        #convert AUTO value to indicate whether we're BUNDLED/SYSTEM
	IF(${optname} MATCHES "AUTO")
		IF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "BUNDLED")
			SET(${optname} "BUNDLED (AUTO)" CACHE STRING "Using bundled ${optname}" FORCE)
		ENDIF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "BUNDLED")

		IF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "SYSTEM")
			SET(${optname} "SYSTEM (AUTO)" CACHE STRING "Using system ${optname}" FORCE)
		ENDIF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "SYSTEM")

		IF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "AUTO")
			SET(${optname} "AUTO" CACHE STRING "Using bundled ${optname}" FORCE)
		ENDIF(${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS} MATCHES "AUTO")
	ENDIF(${optname} MATCHES "AUTO")

	set_property(CACHE ${optname} PROPERTY STRINGS AUTO BUNDLED SYSTEM)

        # make sure we have a valid value
	IF(NOT ${${optname}} MATCHES "AUTO" AND NOT ${${optname}} MATCHES "BUNDLED" AND NOT ${${optname}} MATCHES "SYSTEM")
		MESSAGE(WARNING "Unknown value ${${optname}} supplied for ${optname} - defaulting to AUTO\nValid options are AUTO, BUNDLED and SYSTEM")
		SET(${optname} "AUTO" CACHE STRING "Using bundled ${optname}" FORCE)
	ENDIF(NOT ${${optname}} MATCHES "AUTO" AND NOT ${${optname}} MATCHES "BUNDLED" AND NOT ${${optname}} MATCHES "SYSTEM")
ENDMACRO()


#-----------------------------------------------------------------------------
# Build Type aware option
MACRO(AUTO_OPTION username varname debug_state release_state)
	STRING(LENGTH "${${username}}" ${username}_SET)

	IF(NOT ${username}_SET)
		SET(${username} "AUTO" CACHE STRING "auto option" FORCE)
	ENDIF(NOT ${username}_SET)

	set_property(CACHE ${username} PROPERTY STRINGS AUTO "ON" "OFF")

	STRING(TOUPPER "${${username}}" username_upper)
	SET(${username} ${username_upper})

	IF(NOT ${${username}} MATCHES "AUTO" AND NOT ${${username}} MATCHES "ON" AND NOT ${${username}} MATCHES "OFF")
		MESSAGE(WARNING "Unknown value ${${username}} supplied for ${username} - defaulting to AUTO.\nValid options are AUTO, ON and OFF")
		SET(${username} "AUTO" CACHE STRING "auto option" FORCE)
	ENDIF(NOT ${${username}} MATCHES "AUTO" AND NOT ${${username}} MATCHES "ON" AND NOT ${${username}} MATCHES "OFF")

	# If the "parent" setting isn't AUTO, do what it says
	IF(NOT ${${username}} MATCHES "AUTO")
		SET(${varname} ${${username}})
	ENDIF(NOT ${${username}} MATCHES "AUTO")

	# If we we don't understand the build type and have an AUTO setting for the
	# optimization flags, leave them off
	IF(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
		IF(NOT ${${username}} MATCHES "AUTO")
			SET(${varname} ${${username}})
		ELSE(NOT ${${username}} MATCHES "AUTO")
			SET(${varname} OFF)
			SET(${username} "OFF (AUTO)" CACHE STRING "auto option" FORCE)
		ENDIF(NOT ${${username}} MATCHES "AUTO")
	ENDIF(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")

	# If we DO understand the build type and have AUTO, be smart
	IF("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${${username}} MATCHES "AUTO")
		SET(${varname} ${release_state})
		SET(${username} "${release_state} (AUTO)" CACHE STRING "auto option" FORCE)
	ENDIF("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${${username}} MATCHES "AUTO")

	IF("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${${username}} MATCHES "AUTO")
		SET(${varname} ${debug_state})
		SET(${username} "${debug_state} (AUTO)" CACHE STRING "auto option" FORCE)
	ENDIF("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${${username}} MATCHES "AUTO")
ENDMACRO(AUTO_OPTION varname release_state debug_state)


#-----------------------------------------------------------------------------
# For "top-level" BRL-CAD options, some extra work is in order - descriptions and
# lists of aliases are supplied, and those are automatically addressed by this
# macro.  In this context, "aliases" are variables which may be defined on the
# CMake command line that are intended to set the value of th BRL-CAD option in
# question, but use some other name.  Aliases may be common typos, different
# nomenclature, or anything that the developer things should lead to a given
# option being set.  The documentation is auto-formatted into a text document
# describing all BRL-CAD options.
MACRO(BRLCAD_OPTION thisoption)
	FOREACH(item ${${thisoption}_ALIASES})
		IF(NOT "${item}" STREQUAL "${thisoption}")
			IF(NOT "${${item}}" STREQUAL "")
				SET(${thisoption} ${${item}} CACHE STRING "setting ${thisoption} via alias ${item}" FORCE)
				SET(${item} "" CACHE STRING "set ${thisoption} via this variable" FORCE)
				MARK_AS_ADVANCED(${item})
			ENDIF(NOT "${${item}}" STREQUAL "")
		ENDIF(NOT "${item}" STREQUAL "${thisoption}")
	ENDFOREACH(item ${${thisoption}_ALIASES})

	FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${thisoption}:\n")
	FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${${thisoption}_DESCRIPTION}")

	SET(ALIASES_LIST "\nAliases: ")
	FOREACH(item ${${thisoption}_ALIASES})
		SET(ALIASES_LIST_TEST "${ALIASES_LIST}, ${item}")
		STRING(LENGTH ${ALIASES_LIST_TEST} LL)

		IF(${LL} GREATER 80)
			FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${ALIASES_LIST}\n")
			SET(ALIASES_LIST "          ${item}")
		ELSE(${LL} GREATER 80)
			IF(NOT ${ALIASES_LIST} MATCHES "\nAliases")
				SET(ALIASES_LIST "${ALIASES_LIST}, ${item}")
			ELSE(NOT ${ALIASES_LIST} MATCHES "\nAliases")
				IF(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
					SET(ALIASES_LIST "${ALIASES_LIST}, ${item}")
				ELSE(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
					SET(ALIASES_LIST "${ALIASES_LIST} ${item}")
				ENDIF(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
			ENDIF(NOT ${ALIASES_LIST} MATCHES "\nAliases")
		ENDIF(${LL} GREATER 80)
	ENDFOREACH(item ${${thisoption}_ALIASES})

	IF(ALIASES_LIST)	
		STRING(STRIP ALIASES_LIST ${ALIASES_LIST})
		IF(ALIASES_LIST)	
			FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "${ALIASES_LIST}")
		ENDIF(ALIASES_LIST)	
	ENDIF(ALIASES_LIST)	

	FILE(APPEND ${CMAKE_BINARY_DIR}/OPTIONS "\n\n")
ENDMACRO(BRLCAD_OPTION)

INCLUDE(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)


#-----------------------------------------------------------------------------
MACRO(CPP_WARNINGS srcslist)
	# We need to specify specific flags for c++ files - their warnings are
	# not usually used to hault the build, althogh BRLCAD_ENABLE_CXX_STRICT
	# can be set to on to achieve that
	IF(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
		FOREACH(srcfile ${${srcslist}})
			IF(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
				IF(BRLCAD_ENABLE_COMPILER_WARNINGS)
					IF(NOERROR_FLAG)
						GET_PROPERTY(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
						set_source_files_properties(${srcfile} COMPILE_FLAGS "-Wno-error ${previous_flags}")
					ENDIF(NOERROR_FLAG)
				ELSE(BRLCAD_ENABLE_COMPILER_WARNINGS)
					GET_PROPERTY(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
					set_source_files_properties(${srcfile} COMPILE_FLAGS "-w ${previous_flags}")
				ENDIF(BRLCAD_ENABLE_COMPILER_WARNINGS)
			ENDIF(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
		ENDFOREACH(srcfile ${${srcslist}})
	ENDIF(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
ENDMACRO(CPP_WARNINGS)


#-----------------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
MACRO(BRLCAD_ADDEXEC execname srcs libs)
	STRING(REGEX REPLACE " " ";" srcslist "${srcs}")
	STRING(REGEX REPLACE " " ";" libslist1 "${libs}")
	STRING(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")

	add_executable(${execname} ${srcslist})
	target_link_libraries(${execname} ${libslist})

	install(TARGETS ${execname} DESTINATION ${BIN_DIR})

	FOREACH(libitem ${libslist})
		LIST(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
		IF(NOT ${FOUNDIT} STREQUAL "-1")
			STRING(REGEX REPLACE "lib" "" ITEMCORE "${libitem}")
			STRING(TOUPPER ${ITEMCORE} ITEM_UPPER_CORE)
			LIST(APPEND ${execname}_DEFINES ${${ITEM_UPPER_CORE}_DEFINES})
			LIST(REMOVE_DUPLICATES ${execname}_DEFINES)
		ENDIF(NOT ${FOUNDIT} STREQUAL "-1")
	ENDFOREACH(libitem ${libslist})

	FOREACH(lib_define ${${execname}_DEFINES})
		SET_PROPERTY(TARGET ${execname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
	ENDFOREACH(lib_define ${${execname}_DEFINES})

	FOREACH(extraarg ${ARGN})
		IF(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
			IF(NOERROR_FLAG)
				SET_TARGET_PROPERTIES(${execname} PROPERTIES COMPILE_FLAGS "-Wno-error")
			ENDIF(NOERROR_FLAG)
		ENDIF(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
	ENDFOREACH(extraarg ${ARGN})

	CPP_WARNINGS(srcslist)
ENDMACRO(BRLCAD_ADDEXEC execname srcs libs)


#-----------------------------------------------------------------------------
# Library macro handles both shared and static libs, so one "BRLCAD_ADDLIB"
# statement will cover both automatically
MACRO(BRLCAD_ADDLIB libname srcs libs)

	# Add ${libname} to the list of BRL-CAD libraries	
	LIST(APPEND BRLCAD_LIBS ${libname})
	LIST(REMOVE_DUPLICATES BRLCAD_LIBS)
	SET(BRLCAD_LIBS "${BRLCAD_LIBS}" CACHE STRING "BRL-CAD libraries" FORCE)

	# Define convenience variables and scrub library list	
	STRING(REGEX REPLACE "lib" "" LOWERCORE "${libname}")
	STRING(TOUPPER ${LOWERCORE} UPPER_CORE)
	STRING(REGEX REPLACE " " ";" srcslist "${srcs}")
	STRING(REGEX REPLACE " " ";" libslist1 "${libs}")
	STRING(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")

	# Collect the definitions needed by this library
	# Appending to the list to ensure that any non-template
	# definitions made in the CMakeLists.txt file are preserved.
	# In case of re-running cmake, make sure the DLL_IMPORTS define
	# for this specific library is removed, since for the actual target 
	# we need to export, not import.
	LIST(REMOVE_ITEM ${UPPER_CORE}_DEFINES ${UPPER_CORE}_DLL_IMPORTS)
	FOREACH(libitem ${libslist})
		LIST(FIND BRLCAD_LIBS ${libitem} FOUNDIT)

		IF(NOT ${FOUNDIT} STREQUAL "-1")
			STRING(REGEX REPLACE "lib" "" ITEMCORE "${libitem}")
			STRING(TOUPPER ${ITEMCORE} ITEM_UPPER_CORE)
			LIST(APPEND ${UPPER_CORE}_DEFINES ${${ITEM_UPPER_CORE}_DEFINES})
			LIST(REMOVE_DUPLICATES ${UPPER_CORE}_DEFINES)
		ENDIF(NOT ${FOUNDIT} STREQUAL "-1")
	ENDFOREACH(libitem ${libslist})

	# Handle "shared" libraries (with MSVC, these would be dynamic libraries)
	IF(BUILD_SHARED_LIBS)
		add_library(${libname} SHARED ${srcslist})

		IF(CPP_DLL_DEFINES)
			SET_PROPERTY(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
		ENDIF(CPP_DLL_DEFINES)

		IF(NOT ${libs} MATCHES "NONE")
			target_link_libraries(${libname} ${libslist})
		ENDIF(NOT ${libs} MATCHES "NONE")

		INSTALL(TARGETS ${libname} DESTINATION ${LIB_DIR})

		FOREACH(lib_define ${${UPPER_CORE}_DEFINES})
			SET_PROPERTY(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
		ENDFOREACH(lib_define ${${UPPER_CORE}_DEFINES})

		FOREACH(extraarg ${ARGN})
			IF(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
				IF(NOERROR_FLAG)
					SET_PROPERTY(TARGET ${libname} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
				ENDIF(NOERROR_FLAG)
			ENDIF(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
		ENDFOREACH(extraarg ${ARGN})
	ENDIF(BUILD_SHARED_LIBS)

	# For any BRL-CAD libraries using this library, define a variable 
	# with the needed definitions.
	IF(CPP_DLL_DEFINES)
		LIST(APPEND ${UPPER_CORE}_DEFINES ${UPPER_CORE}_DLL_IMPORTS)
	ENDIF(CPP_DLL_DEFINES)
	SET(${UPPER_CORE}_DEFINES "${${UPPER_CORE}_DEFINES}" CACHE STRING "${UPPER_CORE} library required definitions" FORCE)

	# Handle static libraries (renaming requirements to both allow unique targets and
	# respect standard naming conventions.)
	IF(BUILD_STATIC_LIBS)
		add_library(${libname}-static STATIC ${srcslist})

		IF(NOT MSVC)
			IF(NOT ${libs} MATCHES "NONE")
				target_link_libraries(${libname}-static ${libslist})
			ENDIF(NOT ${libs} MATCHES "NONE")
			SET_TARGET_PROPERTIES(${libname}-static PROPERTIES OUTPUT_NAME "${libname}")
		ENDIF(NOT MSVC)

		INSTALL(TARGETS ${libname}-static DESTINATION ${LIB_DIR})

		FOREACH(extraarg ${ARGN})
			IF(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
				IF(NOERROR_FLAG)
					SET_PROPERTY(TARGET ${libname}-static APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
				ENDIF(NOERROR_FLAG)
			ENDIF(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
		ENDFOREACH(extraarg ${ARGN})
	ENDIF(BUILD_STATIC_LIBS)

	CPP_WARNINGS(srcslist)
ENDMACRO(BRLCAD_ADDLIB libname srcs libs)


#-----------------------------------------------------------------------------
# Wrapper to handle include directories specific to libraries.  Removes
# duplicates and makes sure the <LIB>_INCLUDE_DIRS is in the cache
# immediately, so it can be used by other libraries.  These are not
# intended as toplevel user settable options so mark as advanced.
MACRO(BRLCAD_INCLUDE_DIRS DIR_LIST)
	STRING(REGEX REPLACE "_INCLUDE_DIRS" "" LIB_UPPER "${DIR_LIST}")
	STRING(TOLOWER ${LIB_UPPER} LIB_LOWER)

	LIST(REMOVE_DUPLICATES ${DIR_LIST})
	SET(${DIR_LIST} ${${DIR_LIST}} CACHE STRING "Include directories for lib${LIBLOWER}" FORCE)

	MARK_AS_ADVANCED(${DIR_LIST})

	include_directories(${${DIR_LIST}})
ENDMACRO(BRLCAD_INCLUDE_DIRS)


#-----------------------------------------------------------------------------
# We attempt here to strike a balance between competing needs.  Ideally, any error messages
# returned as a consequence of using data while running programs should point the developer
# back to the version controlled source code, not a copy in the build directory.  However,
# another design goal is to replicate the install tree layout in the build directory.  On
# platforms with symbolic links, we can do both by linking the source data files to their
# locations in the build directory.  On Windows, this is not possible and we have to fall
# back on an explicit file copy mechanism.  In both cases we have a build target that is
# triggered if source files are edited in order to allow the build system to take any further
# actions that may be needed (the current example is re-generating tclIndex and pkgIndex.tcl
# files when the source .tcl files change).

# In principle it may be possible to go even further and add rules to copy files in the build
# dir that are different from their source originals back into the source tree... need to
# think about complexity/robustness tradeoffs
MACRO(BRLCAD_ADDDATA datalist targetdir)
	IF(NOT WIN32)
		IF(NOT EXISTS "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")
			EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		ENDIF(NOT EXISTS "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")

		FOREACH(filename ${${datalist}})
			GET_FILENAME_COMPONENT(ITEM_NAME ${filename} NAME)
			EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
		ENDFOREACH(filename ${${datalist}})

		STRING(REGEX REPLACE "/" "_" targetprefix ${targetdir})

		ADD_CUSTOM_COMMAND(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
			DEPENDS ${${datalist}}
			)
		ADD_CUSTOM_TARGET(${datalist}_cp ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel)
	ELSE(NOT WIN32)
		FILE(COPY ${${datalist}} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		STRING(REGEX REPLACE "/" "_" targetprefix ${targetdir})
		SET(inputlist)

		FOREACH(filename ${${datalist}})
			SET(inputlist ${inputlist} ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
		ENDFOREACH(filename ${${datalist}})

		SET(${targetprefix}_cmake_contents "
		SET(FILES_TO_COPY \"${inputlist}\")
		FILE(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}\")
		")
		FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.cmake "${${targetprefix}_cmake_contents}")
		ADD_CUSTOM_COMMAND(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
			COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.cmake
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
			DEPENDS ${${datalist}}
			)
		ADD_CUSTOM_TARGET(${datalist}_cp ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel)
	ENDIF(NOT WIN32)

	FOREACH(filename ${${datalist}})
		INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${filename} DESTINATION ${DATA_DIR}/${targetdir})
	ENDFOREACH(filename ${${datalist}})
	CMAKEFILES(${${datalist}})
ENDMACRO(BRLCAD_ADDDATA datalist targetdir)


#-----------------------------------------------------------------------------
MACRO(BRLCAD_ADDFILE filename targetdir)
	FILE(COPY ${filename} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
	STRING(REGEX REPLACE "/" "_" targetprefix ${targetdir})

	IF(BRLCAD_ENABLE_DATA_TARGETS)
		STRING(REGEX REPLACE "/" "_" filestring ${filename})
		ADD_CUSTOM_COMMAND(
			OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename}
			COMMAND ${CMAKE_COMMAND} -E copy	${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		ADD_CUSTOM_TARGET(${targetprefix}_${filestring}_cp ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename})
	ENDIF(BRLCAD_ENABLE_DATA_TARGETS)

	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${filename} DESTINATION ${DATA_DIR}/${targetdir})

	FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${filename}\n")
ENDMACRO(BRLCAD_ADDFILE datalist targetdir)


#-----------------------------------------------------------------------------
MACRO(DISTCHECK_IGNORE_ITEM itemtoignore)
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
		IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
			FILE(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore}\n")
		ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
			FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore}\n")
		ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
	ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
		MESSAGE(FATAL_ERROR "Attempting to ignore non-existent file ${itemtoignore}")
	ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${itemtoignore})
ENDMACRO(DISTCHECK_IGNORE_ITEM)


#-----------------------------------------------------------------------------
MACRO(DISTCHECK_IGNORE targetdir filestoignore)
	FOREACH(ITEM ${${filestoignore}})
		get_filename_component(ITEM_PATH ${ITEM} PATH)
		get_filename_component(ITEM_NAME ${ITEM} NAME)

		IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
			IF(NOT ITEM_PATH STREQUAL "")
				GET_FILENAME_COMPONENT(ITEM_ABS_PATH ${targetdir}/${ITEM_PATH} ABSOLUTE)

				IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
					FILE(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
				ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
					FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}/${ITEM_NAME}\n")
				ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})

				FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")

				WHILE(NOT ITEM_PATH STREQUAL "")
					get_filename_component(ITEM_NAME ${ITEM_PATH} NAME)
					get_filename_component(ITEM_PATH ${ITEM_PATH} PATH)
					IF(NOT ITEM_PATH STREQUAL "")
						GET_FILENAME_COMPONENT(ITEM_ABS_PATH ${targetdir}/${ITEM_PATH} ABSOLUTE)
						IF(NOT ${ITEM_NAME} MATCHES "\\.\\.")
							FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS_PATH}\n")
						ENDIF(NOT ${ITEM_NAME} MATCHES "\\.\\.")
					ENDIF(NOT ITEM_PATH STREQUAL "")
				ENDWHILE(NOT ITEM_PATH STREQUAL "")
			ELSE(NOT ITEM_PATH STREQUAL "")
				IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
					FILE(APPEND ${CMAKE_BINARY_DIR}/cmakedirs.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}\n")
				ELSE(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
					FILE(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}\n")
				ENDIF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
			ENDIF(NOT ITEM_PATH STREQUAL "")
		ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
			MESSAGE(FATAL_ERROR "Attempting to ignore non-existent file ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM}")
		ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${targetdir}/${ITEM})
	ENDFOREACH(ITEM ${${filestoignore}})
ENDMACRO(DISTCHECK_IGNORE)


#-----------------------------------------------------------------------------
MACRO(ADD_MAN_PAGES num manlist)
	CMAKEFILES(${${manlist}})
	FOREACH(manpage ${${manlist}})
		install(FILES ${manpage} DESTINATION ${MAN_DIR}/man${num})
	ENDFOREACH(manpage ${${manlist}})
ENDMACRO(ADD_MAN_PAGES num manlist)


#-----------------------------------------------------------------------------
MACRO(ADD_MAN_PAGE num manfile)
	CMAKEFILES(${manfile})
	FILE(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmakefiles.cmake "${manfile}\n")
	install(FILES ${manfile} DESTINATION ${MAN_DIR}/man${num})
ENDMACRO(ADD_MAN_PAGE num manfile)

