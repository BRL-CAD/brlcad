if(TARGET librt AND BUILD_TESTING)
  brlcad_addexec(rt_primitives rt_primitives.c "librt;libbu;libbn" NO_INSTALL)

  # The real brep sample is carried as serialized NURBS in primitives.asc and
  # round-trips through asc2g (Tcl), but the gcv fallback importer used when
  # Tcl is disabled may not accept it.  Relax the brep requirement in that
  # configuration so the test degrades gracefully instead of failing.  (nmg,
  # sph and rec are unconditionally excluded inside the tool itself.)
  # 
  # hacky - need to validate this before having a whole special compile def
  if(NOT BRLCAD_ENABLE_TCL)
    target_compile_definitions(rt_primitives PRIVATE RTP_NO_TCL)
  endif(NOT BRLCAD_ENABLE_TCL)

  # Registers only once primitives.g (generated from db/primitives.asc) exists
  # as a target; runs rt_primitives against the built share/db/primitives.g.
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-rt_primitives.log")
  brlcad_regression_test(regress-rt_primitives "rt_primitives;primitives.g" EXEC rt_primitives)
  distclean("${LOG_FILE}")
endif(TARGET librt AND BUILD_TESTING)

cmakefiles(
  CMakeLists.txt
  rt_primitives.c
  rt_report.h
  regress-rt_primitives.cmake.in
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
