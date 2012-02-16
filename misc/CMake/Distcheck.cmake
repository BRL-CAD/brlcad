# Each config (Debug, Release, Tk disable, etc. etc.) is set up as an
# individual distcheck target (distcheck-debug, distcheck-release, etc.) and then toplevel distcheck
# and distcheck-full targets

if("${CMAKE_VERBOSE_DISTCHECK}" STREQUAL "")
	set(CMAKE_VERBOSE_DISTCHECK OFF)
endif("${CMAKE_VERBOSE_DISTCHECK}" STREQUAL "")

macro(CREATE_DISTCHECK TARGET_SUFFIX CMAKE_OPTS)
	# Need to set these locally so configure_file will pick them up...
	SET(TARGET_SUFFIX ${TARGET_SUFFIX})
	SET(CMAKE_OPTS ${CMAKE_OPTS})

    # Determine how to trigger the build in the distcheck target
    if("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
		 if(NOT CMAKE_VERBOSE_DISTCHECK)
		   set(TARGET_REDIRECT " >> distcheck_${TARGET_SUFFIX}.log ")
		 endif(NOT CMAKE_VERBOSE_DISTCHECK)
		 set(DISTCHECK_BUILD_CMD "${CMAKE_COMMAND} -E chdir  distcheck-${TARGET_SUFFIX}/build $(MAKE) ${TARGET_REDIRECT}")
    else("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
		 set(DISTCHECK_BUILD_CMD "COMMAND ${CMAKE_COMMAND} -E build distcheck-${TARGET_SUFFIX}/build")
		 set(TARGET_REDIRECT "")
    endif("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))

    # Based on the build command, generate a distcheck target definition from the template
	 configure_file(${BRLCAD_CMAKE_DIR}/distcheck_target.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target_${TARGET_SUFFIX}.cmake @ONLY)
	 include(${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target_${TARGET_SUFFIX}.cmake)

	 # Keep track of the distcheck targets
	 set(distcheck_targets ${distcheck_targets} distcheck-${TARGET_SUFFIX})
endmacro(CREATE_DISTCHECK)

macro(DEFINE_DISTCHECK_TARGET)
  if(NOT IS_SUBBUILD)
    find_program(CPACK_EXEC cpack)
    mark_as_advanced(CPACK_EXEC)

    # Set up the script that will be used to verify the source archives
    configure_file(${BRLCAD_CMAKE_DIR}/distcheck_repo_verify.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_repo_verify.cmake @ONLY)

	 add_custom_target(distcheck-repo-verify
		 COMMAND ${CMAKE_COMMAND} -E echo "*** Check files in Source Repository against files specified in Build Logic ***"
		 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_repo_verify.cmake)

	 add_custom_target(distcheck-source-archives
		 COMMAND ${CMAKE_COMMAND} -E echo "*** Create source tgz, tbz2 and zip archives from toplevel archive ***"
		 COMMAND ${CPACK_EXEC} --config ${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake
		 DEPENDS distcheck-repo-verify)


	 CREATE_DISTCHECK(debug   "-DCMAKE_BUILD_TYPE=Debug")
	 CREATE_DISTCHECK(release "-DCMAKE_BUILD_TYPE=Release")

	 add_custom_target(distcheck 
	    # The source repository verification script is responsible for generating this file
       COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_message
		 DEPENDS distcheck-debug distcheck-release)

	 add_custom_target(distcheck-full
       # The source repository verification script is responsible for generating this file
       COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_message
		 DEPENDS ${distcheck_targets})

  endif(NOT IS_SUBBUILD)
endmacro(DEFINE_DISTCHECK_TARGET)
