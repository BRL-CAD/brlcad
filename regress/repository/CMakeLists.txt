if (DEFINED REPOCHECK_TEST)
  add_executable(repocheck repocheck.cpp)
  if (O3_COMPILER_FLAG)
    # This check benefits greatly from optimization. If we have the O3 flag, use
    # it - whether or not the standard build settings would add it.
    target_compile_options(lcheck PRIVATE "-O3")
  endif (O3_COMPILER_FLAG)
  set_target_properties(repocheck PROPERTIES FOLDER "BRL-CAD Regression Tests")


  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-repository.log")
  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-repository.done")
  set(ALL_FILES_LIST "${BRLCAD_BINARY_DIR}/cmakefiles.cmake")

  add_custom_command(
    OUTPUT "${STAMP_FILE}"
    COMMAND "${CMAKE_COMMAND}"
    -DLCHECK_EXEC="$<TARGET_FILE:repocheck>"
    -DL_FILE="${LOG_FILE}" -DS_FILE="${STAMP_FILE}" -DF_LIST="${ALL_FILES_LIST}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/regress-repository.cmake"
    )
  add_custom_target(regress-repository DEPENDS ${STAMP_FILE} ${CMAKE_BINARY_DIR}/embedded_repository.txt)
  set_target_properties(regress-repository PROPERTIES FOLDER "BRL-CAD Regression Tests")
  add_dependencies(regress regress-repository)
  add_dependencies(check regress-repository)

  add_test(NAME regress-repository
    COMMAND "${CMAKE_COMMAND}"
    -DLCHECK_EXEC=$<TARGET_FILE:repocheck>
    -DL_FILE=${LOG_FILE} -DS_FILE=${STAMP_FILE} -DF_LIST=${ALL_FILES_LIST}
    -DW_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/regress-repository.cmake"
    )
  set_tests_properties(regress-repository PROPERTIES LABELS "Regression")

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    )

  set_target_properties(regress-repository PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
endif (DEFINED REPOCHECK_TEST)

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
