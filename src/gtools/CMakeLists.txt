set(LDIR "${BRLCAD_SOURCE_DIR}/src/other/linenoise")

set(GTOOLS_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${LDIR}
  )
list(REMOVE_DUPLICATES GTOOLS_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(GTOOLS_INCLUDE_DIRS)

BRLCAD_ADDDATA(gtransfer.c sample_applications)
BRLCAD_ADDEXEC(gex "gex.cpp" "libbu;librt")
BRLCAD_ADDEXEC(glint glint.cpp "librt;${M_LIBRARY}")
BRLCAD_ADDEXEC(gqa gqa.c "libged;librt")
BRLCAD_ADDEXEC(gchecker gchecker.cpp "libged;librt;libbu")

set(GSH_SRCS
  gsh.cpp
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  )
BRLCAD_ADDEXEC(gsh "${GSH_SRCS}"  "libged;libdm;libbu" NO_INSTALL)
BRLCAD_ADDEXEC(gtransfer gtransfer.c "librt;libpkg" NO_INSTALL)

set(GANALYZE_SRCS
  ganalyze.cpp
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  )
BRLCAD_ADDEXEC(ganalyze "${GANALYZE_SRCS}" "libanalyze;librt;libbu;libpkg" NO_INSTALL)

add_subdirectory(beset)
add_subdirectory(gdiff)

add_subdirectory(tests)


CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
