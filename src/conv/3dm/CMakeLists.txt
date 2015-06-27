if(MSVC)
  add_definitions(-DON_DLL_IMPORTS)
endif(MSVC)

set(3DM_INCLUDE_DIRS
  ${OPENNURBS_INCLUDE_DIRS}
  ${GCV_INCLUDE_DIRS}
  ${ICV_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(3DM_INCLUDE_DIRS)

BRLCAD_ADDEXEC(3dm-g 3dm-g.cpp "libgcv;libwdb;libicv;${OPENNURBS_LIBRARY}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
