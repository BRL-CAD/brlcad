set(PLY_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(PLY_INCLUDE_DIRS)

BRLCAD_ADDEXEC(ply-g "ply-g.c;rply.c" "librt;libnmg;libbu;libwdb" FOLDER Conv)
BRLCAD_ADDEXEC(g-ply "g-ply.c;rply.c" "librt;libnmg;libbu" FOLDER Conv)

if (HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET ply-g APPEND PROPERTY COMPILE_DEFINITIONS "RPLY_DLL_IMPORTS")
  set_property(TARGET g-ply APPEND PROPERTY COMPILE_DEFINITIONS "RPLY_DLL_IMPORTS")
endif (HIDE_INTERNAL_SYMBOLS)

CMAKEFILES(
  CMakeLists.txt
  rply.h
  rplyfile.h
  rply_license.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

