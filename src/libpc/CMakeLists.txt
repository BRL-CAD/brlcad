# Include directories needed by libpc users
set(PC_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

set(pc_ignore_files
  solver_test.cpp
  vm_test.cpp
  )
CMAKEFILES(${pc_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
