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
include_directories(${GTOOLS_INCLUDE_DIRS})

BRLCAD_ADDEXEC(glint glint.c "librt;${M_LIBRARY}")
BRLCAD_ADDEXEC(gqa gqa.c "libged;librt")
BRLCAD_ADDEXEC(remapid remapid.c "librt;libbu")

BRLCAD_ADDEXEC(gtransfer gtransfer.c "librt;libpkg" NO_INSTALL)
BRLCAD_ADDDATA(gtransfer.c sample_applications)

BRLCAD_ADDEXEC(geval geval.c "libged;libbu" NO_INSTALL)

set(GSH_SRCS
  gsh.cxx
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  )
BRLCAD_ADDEXEC(gsh "${GSH_SRCS}"  "libged;libbu" NO_INSTALL)

add_subdirectory(beset)
add_subdirectory(gdiff)

add_subdirectory(tests)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
