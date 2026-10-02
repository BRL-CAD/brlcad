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

if(BRLCAD_ENABLE_TCL)
  brlcad_addexec(tcl_source_lint "tcl_source_lint.cpp" "libbu;${TCL_LIBRARY}" TEST)
  target_include_directories(tcl_source_lint PRIVATE ${TCL_INCLUDE_PATH})
  set_target_properties(tcl_source_lint PROPERTIES FOLDER "BRL-CAD Regression Tests/repository")
endif(BRLCAD_ENABLE_TCL)
distclean(${LOG_FILE})

if(TARGET btclsh AND TARGET TclIndexBld AND TARGET tcl_source_lint)
  brlcad_regression_test(
    regress-tcl-namespace-policy
    "btclsh;TclIndexBld;tcl_source_lint"
    EXEC btclsh
    VEXEC tcl_source_lint
  )
  if(TARGET regress-repository)
    add_dependencies(regress-repository regress-tcl-namespace-policy)
  endif()
  distclean(${CMAKE_CURRENT_BINARY_DIR}/regress-tcl-namespace-policy.log)
endif()

cmakefiles(
  CMakeLists.txt
  repocheck.cpp
  regress-repository.cmake.in
  regress-tcl-namespace-policy.cmake.in
  tcl_namespace_manifest.txt
  tcl_namespace_policy.tcl
  tcl_source_lint.cpp
  tcl_source_lint_fixture.tcl
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
