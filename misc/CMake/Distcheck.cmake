macro(DEFINE_DISTCHECK_TARGET)
  if(NOT IS_SUBBUILD)
    find_program(CPACK_EXEC cpack)
    mark_as_advanced(CPACK_EXEC)

    # Set up the script that will be used to verify the source archives
    configure_file(${BRLCAD_CMAKE_DIR}/distcheck_buildsys.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_buildsys.cmake @ONLY)

    # Determine how to trigger the build in the distcheck target
    if("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
      set(DISTCHECK_BUILD_CMD "${CMAKE_COMMAND} -E chdir _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build $(MAKE) > distcheck_compilation_log")
    else("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
      set(DISTCHECK_BUILD_CMD "COMMAND ${CMAKE_COMMAND} -E build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build")
    endif("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))

    # Based on the build command, generate a distcheck target definition from the template
    configure_file(${BRLCAD_CMAKE_DIR}/distcheck_target.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target.cmake @ONLY)
    include(${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_target.cmake)

  endif(NOT IS_SUBBUILD)
endmacro(DEFINE_DISTCHECK_TARGET)
