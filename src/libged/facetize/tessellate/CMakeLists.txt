set(GED_TESS_SRCS
  continuation.cpp
  sample.cpp
  spsr.cpp
  main.cpp
  nmg.cpp
  util.cpp
  )

BRLCAD_ADDEXEC(ged_tessellate "${GED_TESS_SRCS}" "libged;${MANIFOLD_LIBRARIES}")

CMAKEFILES(
  CMakeLists.txt
  tessellate.h
  ${GED_TESS_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
