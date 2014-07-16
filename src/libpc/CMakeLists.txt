# Include directories needed by libpc users
set(PC_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${GECODE_INCLUDE_DIR}
  )

set(PC_LOCAL_INCLUDE_DIRS)

BRLCAD_LIB_INCLUDE_DIRS(pc PC_INCLUDE_DIRS PC_LOCAL_INCLUDE_DIRS)

BRLCAD_ADDEXEC(cartesian-heart cartesian-heart.cpp "${GECODE_LIBRARIES}" NO_INSTALL)
BRLCAD_ADDEXEC(gecode_solver_test gecode_solver_test.cpp "${GECODE_LIBRARIES}" NO_INSTALL)
BRLCAD_ADDEXEC(gecode_librt_test gecode_librt_test.cpp "${GECODE_LIBRARIES}" NO_INSTALL)

set(pc_ignore_files
  solver_test.cpp
  vm_test.cpp
  domain_constraint_propagator.cpp
  )
CMAKEFILES(${pc_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
