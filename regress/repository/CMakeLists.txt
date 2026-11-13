include_directories(${BRLCAD_BINARY_DIR}/include ${BRLCAD_SOURCE_DIR}/include)

# Build a scanner to check the repository for various problems
brlcad_addexec(repocheck "repocheck.cpp" "libbu;Threads::Threads" TEST)
if(TARGET repocheck)
  if(O3_COMPILER_FLAG)
    # This check benefits greatly from optimization. If we have the O3 flag, use
    # it - whether or not the standard build settings would add it.
    target_compile_options(repocheck PRIVATE "-O3")
  endif(O3_COMPILER_FLAG)
  set_target_properties(repocheck PROPERTIES FOLDER "BRL-CAD Regression Tests/repository")
endif(TARGET repocheck)

# Define the actual regression test
set(ALL_FILES_LIST "${BRLCAD_BINARY_DIR}/cmakefiles.cmake")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-repository.log")
if(TARGET repocheck)
  brlcad_regression_test(regress-repository "repocheck" EXEC repocheck)
endif(TARGET repocheck)
distclean(${LOG_FILE})

# Exercise ELF lookup with a conflicting installation still present.
if(BUILD_TESTING AND (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_test(NAME regress-rpath COMMAND "${CMAKE_COMMAND}"
    "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/rpath-validation"
    "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    -P "${BRLCAD_SOURCE_DIR}/misc/CMake/rpath_validation/validate.cmake")
  brlcad_regression_test(regress-rpath "" TEST_DEFINED)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/rpath-validation")
endif()

cmakefiles(
  CMakeLists.txt
  repocheck.cpp
  regress-repository.cmake.in
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
