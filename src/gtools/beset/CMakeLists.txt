set(BESET_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
list(REMOVE_DUPLICATES BESET_INCLUDE_DIRS)
include_directories(${BESET_INCLUDE_DIRS})

set(beset_SRCS
  beset.c
  fitness.c
  population.c
  )

BRLCAD_ADDEXEC(beset "${beset_SRCS}"  "libbu;librt;libwdb" NO_INSTALL)

set(beset_ignore_files
  beset.h
  fitness.h
  population.h
  )
CMAKEFILES(${beset_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
