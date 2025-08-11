if(BRLCAD_ENABLE_GECODE)
  set(PC_INCLUDE_DIRS ${BRLCAD_BINARY_DIR}/include ${BRLCAD_SOURCE_DIR}/include ${GECODE_INCLUDE_DIR})
  brlcad_addexec(cartesian-heart cartesian-heart.cpp "${GECODE_LIBRARIES}" NO_INSTALL)
  target_include_directories(cartesian-heart BEFORE PRIVATE ${PC_INCLUDE_DIRS})
  brlcad_addexec(gecode_solver_test gecode_solver_test.cpp "${GECODE_LIBRARIES}" NO_INSTALL)
  target_include_directories(gecode_solver_test BEFORE PRIVATE ${PC_INCLUDE_DIRS})
  brlcad_addexec(gecode_librt_test gecode_librt_test.cpp "${GECODE_LIBRARIES}" NO_INSTALL)
  target_include_directories(gecode_librt_test BEFORE PRIVATE ${PC_INCLUDE_DIRS})
endif(BRLCAD_ENABLE_GECODE)

set(
  pc_ignore_files
  CMakeLists.txt
  README
  solver_test.cpp
  vm_test.cpp
  cartesian-heart.cpp
  gecode_solver_test.cpp
  gecode_librt_test.cpp
  perpendicular.fzn
  NOTES
  domain_constraint_propagator.cpp
)
cmakefiles(${pc_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
