include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

add_executable(lcheck licenses_check.cpp)
if (O3_COMPILER_FLAG)
  # This check benefits greatly from optimization. If we have the O3 flag, use
  # it - whether or not the standard build settings would add it.
  target_compile_options(lcheck PRIVATE "-O3")
endif (O3_COMPILER_FLAG)
set_target_properties(lcheck PROPERTIES FOLDER "BRL-CAD Regression Tests/licenses")

# Get the list of embedded license files from doc/legal/embedded and generate
# an input file for the regression test
get_directory_property(ELEGAL DIRECTORY ${CMAKE_SOURCE_DIR}/doc/legal/embedded DEFINITION embedded_licenses)
set(LICENSE_LIST "${CMAKE_CURRENT_BINARY_DIR}/embedded_licenses.txt")
file(REMOVE "${LICENSE_LIST}")
foreach(lfile ${ELEGAL})
  file(APPEND "${LICENSE_LIST}" "${CMAKE_SOURCE_DIR}/doc/legal/embedded/${lfile}\n")
endforeach(lfile ${ELEGAL})
DISTCLEAN("${LICENSE_LIST}")

set(ALL_FILES_LIST "${BRLCAD_BINARY_DIR}/cmakefiles.cmake")

add_test(NAME regress-licenses
  COMMAND "${CMAKE_COMMAND}"
           -DLCHECK_EXEC=$<TARGET_FILE:lcheck> -DBRLCAD_SOURCE_DIR=${BRLCAD_SOURCE_DIR}
	   -DL_LIST=${LICENSE_LIST} -DF_LIST=${ALL_FILES_LIST}
           -P "${CMAKE_CURRENT_SOURCE_DIR}/regress-licenses.cmake"
  )

if (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-licenses COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -R ^regress-licenses --output-on-failure)
else (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-licenses COMMAND ${CMAKE_CTEST_COMMAND} -R ^regress-licenses --output-on-failure)
endif (CMAKE_CONFIGURATION_TYPES)

Add_Regression_Test(licenses "lcheck")

CMAKEFILES(
  CMakeLists.txt
  licenses_check.cpp
  regress-licenses.cmake
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
