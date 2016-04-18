# Used a variation on Fraser's approach for capturing command line args from
# http://stackoverflow.com/questions/10205986/how-to-capture-cmake-command-line-arguments
# to log what variables have been passed in from the user via -D arguments - haven't
# found a variable that saves the original ARGV list except for those defined in
# -P script mode, which doesn't help here.
get_cmake_property(VARS VARIABLES)
foreach(VAR ${VARS})
  get_property(VAR_HELPSTRING CACHE ${VAR} PROPERTY HELPSTRING)
  # Rather than look for "No help, variable specified on the command line."
  # exactly, match a slightly more robust subset...
  string(TOLOWER "${VAR_HELPSTRING}" VAR_HELPSTRING)
  if("${VAR_HELPSTRING}" MATCHES "specified on the command line")
    get_property(VAR_TYPE CACHE ${VAR} PROPERTY TYPE)
    if(NOT VAR_TYPE STREQUAL "UNINITIALIZED")
      set(VAR "${VAR}:${VAR_TYPE}")
    endif(NOT VAR_TYPE STREQUAL "UNINITIALIZED")
    set(CMAKE_ARGS "${CMAKE_ARGS} -D${VAR}=${${VAR}}")
  endif("${VAR_HELPSTRING}" MATCHES "specified on the command line")
endforeach(VAR ${VARS})
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CMakeOutput.log" "${CMAKE_COMMAND} \"${CMAKE_SOURCE_DIR}\" ${CMAKE_ARGS}\n")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

