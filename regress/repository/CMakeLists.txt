include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

add_executable(repocheck repocheck.cpp)
target_link_libraries(repocheck libbu ${CMAKE_THREAD_LIBS_INIT})
if (O3_COMPILER_FLAG)
  # This check benefits greatly from optimization. If we have the O3 flag, use
  # it - whether or not the standard build settings would add it.
  target_compile_options(repocheck PRIVATE "-O3")
endif (O3_COMPILER_FLAG)
set_target_properties(repocheck PROPERTIES FOLDER "BRL-CAD Regression Tests/repository")

set(ALL_FILES_LIST "${BRLCAD_BINARY_DIR}/cmakefiles.cmake")

add_test(NAME regress-repository
  COMMAND "${CMAKE_COMMAND}"
  -DRCHECK_EXEC=$<TARGET_FILE:repocheck>
  -DL_FILE=${LOG_FILE} -DS_FILE=${STAMP_FILE} -DF_LIST=${ALL_FILES_LIST}
  -DB_DIR=${BRLCAD_SOURCE_DIR}
  -P "${CMAKE_CURRENT_SOURCE_DIR}/regress-repository.cmake"
  )

if (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-repository COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -R ^regress-repository --output-on-failure)
else (CMAKE_CONFIGURATION_TYPES)
  add_custom_target(regress-repository COMMAND ${CMAKE_CTEST_COMMAND} -R ^regress-repository --output-on-failure)
endif (CMAKE_CONFIGURATION_TYPES)

Add_Regression_Test(repository "repocheck")

CMAKEFILES(
  CMakeLists.txt
  repocheck.cpp
  regress-repository.cmake
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
