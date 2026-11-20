include_guard(GLOBAL)
# Call after include(CPack). This adds an optional packaging route without
# changing the project's existing generators or requiring a CMake fork.
function(brlinst_add_package)
  cmake_parse_arguments(BRLI "FILTER_SHORTCUTS" "CONFIG;EXECUTABLE" "FORMATS" ${ARGN})
  if(NOT BRLI_CONFIG)
    message(FATAL_ERROR "brlinst_add_package requires CONFIG")
  endif()
  if(NOT BRLI_EXECUTABLE)
    find_program(BRLI_EXECUTABLE NAMES brlinst REQUIRED)
  endif()
  get_filename_component(BRLINST_CONFIG "${BRLI_CONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  set(BRLINST_EXECUTABLE "${BRLI_EXECUTABLE}")
  if(NOT BRLI_FORMATS)
    set(BRLI_FORMATS exe)
  endif()
  list(REMOVE_DUPLICATES BRLI_FORMATS)
  foreach(BRLINST_FORMAT IN LISTS BRLI_FORMATS)
    if(NOT BRLINST_FORMAT MATCHES "^(exe|msi)$")
      message(FATAL_ERROR "brlinst format must be exe or msi")
    endif()
    if(BRLINST_FORMAT STREQUAL "msi" AND NOT CMAKE_HOST_WIN32)
      message(FATAL_ERROR "MSI generation requires running brlinst on Windows")
    endif()
    if(BRLINST_FORMAT STREQUAL "exe")
      set(_brlinst_target package-brlinst)
    else()
      set(_brlinst_target package-brlinst-msi)
    endif()
    set(_brlinst_script "${CMAKE_CURRENT_BINARY_DIR}/BrlinstPackage-${BRLINST_FORMAT}.cmake")
    set(BRLINST_FORMATS "${BRLINST_FORMAT}")
    set(BRLINST_HOST_BINARY_DIR)
    set(BRLINST_HOST_CONFIG)
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Package.cmake.in" "${_brlinst_script}" @ONLY)
    add_custom_target(${_brlinst_target}
      COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
        -C "$<CONFIG>" -G External
        -D CPACK_EXTERNAL_ENABLE_STAGING=ON
        -D "CPACK_EXTERNAL_PACKAGE_SCRIPT=${_brlinst_script}"
        -D "CPACK_TOPLEVEL_TAG=brlinst-${BRLINST_FORMAT}"
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      USES_TERMINAL VERBATIM
      COMMENT "Generating the brlinst ${BRLINST_FORMAT} installer")
  endforeach()
endfunction()
