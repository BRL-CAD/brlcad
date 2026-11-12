set(ART_INSTALLED_PREFIX "" CACHE PATH "Installed art application and renderer prefix")
set(ART_SCENE_TEST "" CACHE FILEPATH "Development art_scene_test fixture and image checker")
set(LIB_DIR lib CACHE STRING "Installed library subdirectory")
if(NOT IS_DIRECTORY "${ART_INSTALLED_PREFIX}")
  message(FATAL_ERROR "ART_INSTALLED_PREFIX must name an installed art application")
endif()
if(NOT EXISTS "${ART_SCENE_TEST}" OR IS_DIRECTORY "${ART_SCENE_TEST}")
  message(FATAL_ERROR "ART_SCENE_TEST must name the development fixture and image checker")
endif()
file(REAL_PATH "${ART_INSTALLED_PREFIX}" renderer_prefix)
if(NOT EXISTS "${renderer_prefix}/shaders/appleseed/as_plastic.oso")
  message(FATAL_ERROR "The installed art prefix is missing its default shader library")
endif()
unset(ART_EXECUTABLE CACHE)
find_program(ART_EXECUTABLE NAMES art
  HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
set(ART_TEST_TOOL_ENV "${CMAKE_COMMAND}" -E env)
set(runtime_environment)
foreach(variable IN ITEMS BRLCAD_ROOT LD_LIBRARY_PATH LD_PRELOAD DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH)
  list(APPEND ART_TEST_TOOL_ENV "--unset=${variable}")
  list(APPEND runtime_environment "${variable}=unset:")
endforeach()
list(APPEND ART_TEST_TOOL_ENV --)
include("${CMAKE_CURRENT_SOURCE_DIR}/art_test_setup.cmake")
add_custom_target(art_installed_fixtures ALL DEPENDS art_test_shaders)

enable_testing()
set(installed_tests ${ART_REGRESSION_TEST_NAMES})
unset(ART_MGED_EXECUTABLE CACHE)
find_program(ART_MGED_EXECUTABLE NAMES mged
  HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH NO_CACHE)
if(ART_MGED_EXECUTABLE)
  list(APPEND installed_tests art-installed-material art-installed-plate)
endif()
foreach(test IN LISTS installed_tests)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${test}.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${test}.cmake" @ONLY)
  add_test(NAME ${test} COMMAND "${CMAKE_COMMAND}" "-DEXEC=${ART_EXECUTABLE}"
    "-DVEXEC=${ART_SCENE_TEST}" -P "${CMAKE_CURRENT_BINARY_DIR}/${test}.cmake")
  set_tests_properties(${test} PROPERTIES TIMEOUT ${ART_REGRESSION_TIMEOUT} LABELS Installed
    ENVIRONMENT_MODIFICATION "${runtime_environment}")
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_program(ART_LDD_EXECUTABLE NAMES ldd REQUIRED)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art_installed_runtime.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/art_installed_runtime.cmake" @ONLY)
  add_test(NAME art-installed-runtime COMMAND "${CMAKE_COMMAND}"
    -P "${CMAKE_CURRENT_BINARY_DIR}/art_installed_runtime.cmake")
  set_tests_properties(art-installed-runtime PROPERTIES TIMEOUT 30 LABELS Installed
    ENVIRONMENT_MODIFICATION "${runtime_environment}")
endif()
