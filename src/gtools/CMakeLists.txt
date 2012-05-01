set(GTOOLS_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${BRLCAD_SOURCE_DIR}/src/gtools/beset
  )
LIST(REMOVE_DUPLICATES GTOOLS_INCLUDE_DIRS)
include_directories(${GTOOLS_INCLUDE_DIRS})

set(beset_SRCS
  beset/beset.c
  beset/fitness.c
  beset/population.c
  )

BRLCAD_ADDEXEC(beset "${beset_SRCS}"  "libbu;librt;libwdb" NO_INSTALL)

BRLCAD_ADDEXEC(g_diff g_diff.c "libtclcad;libged;librt")
BRLCAD_ADDEXEC(g_lint g_lint.c "librt;${M_LIBRARY}")
BRLCAD_ADDEXEC(g_qa g_qa.c "libged;librt")
BRLCAD_ADDEXEC(remapid remapid.c "librt;libbu")

BRLCAD_ADDEXEC(g_transfer g_transfer.c "librt;libpkg" NO_INSTALL)
BRLCAD_ADDDATA(g_transfer.c sample_applications)

set(gtools_MANS
  g_diff.1
  g_lint.1
  remapid.1
  )
ADD_MAN_PAGES(1 gtools_MANS)
set(gtools_ignore_files
  beset/beset.h
  beset/fitness.h
  beset/population.h
  )
CMAKEFILES(${gtools_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
