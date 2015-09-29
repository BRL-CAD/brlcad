include_directories(
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_BINARY_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIR}
)

set(distcheck_files
  nurbs_csg.g
  )

BRLCAD_ADDEXEC(test_brep_ppx ppx.cpp "libbrep"  NO_INSTALL)
add_test(brep_ppx test_brep_ppx)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
