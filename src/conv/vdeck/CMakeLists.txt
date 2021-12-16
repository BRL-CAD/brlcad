set(VDECK_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(VDECK_INCLUDE_DIRS)

BRLCAD_ADDEXEC(vdeck "cgarbs.c;match.c;parsarg.c;vdeck.c" "librt;${M_LIBRARY}" FOLDER Conv)
CMAKEFILES(std.h vextern.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
