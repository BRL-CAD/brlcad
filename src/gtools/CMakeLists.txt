set(GTOOLS_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES GTOOLS_INCLUDE_DIRS)
include_directories(${GTOOLS_INCLUDE_DIRS})

BRLCAD_ADDEXEC(g_diff g_diff.c "libtclcad;librt")
BRLCAD_ADDEXEC(gdiff2 gdiff2/gdiff2.cpp "librt")
BRLCAD_ADDEXEC(g_lint g_lint.c "librt;${M_LIBRARY}")
BRLCAD_ADDEXEC(g_qa g_qa.c "libged;librt")
BRLCAD_ADDEXEC(remapid remapid.c "librt;libbu")

BRLCAD_ADDEXEC(g_transfer g_transfer.c "librt;libpkg" NO_INSTALL)
BRLCAD_ADDDATA(g_transfer.c sample_applications)

add_subdirectory(beset)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
