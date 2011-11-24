SET(GTOOLS_INCLUDE_DIRS
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

SET(beset_SRCS
  beset/beset.c
  beset/fitness.c
  beset/population.c
)

add_executable(beset ${beset_SRCS})
target_link_libraries(beset libbu librt libwdb)
BRLCAD_TARGET_ADD_DEF_LISTS(beset BU_DEFINES RT_DEFINES WDB_DEFINES)

BRLCAD_ADDEXEC(g_diff g_diff.c "libtclcad libged librt")
BRLCAD_ADDEXEC(g_lint g_lint.c "librt ${M_LIBRARY}")
BRLCAD_ADDEXEC(g_qa g_qa.c "libged librt")
BRLCAD_ADDEXEC(remapid remapid.c "librt libbu")

add_executable(g_transfer g_transfer.c)
target_link_libraries(g_transfer librt libpkg)
BRLCAD_TARGET_ADD_DEF_LISTS(g_transfer BU_DEFINES RT_DEFINES WDB_DEFINES)
SET_TARGET_PROPERTIES(g_transfer PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")

BRLCAD_ADDFILE(g_transfer.c sample_applications)

SET(gtools_MANS
  g_diff.1
  g_lint.1
  remapid.1
)
FILE(COPY ${gtools_MANS} DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man1)
ADD_MAN_PAGES(1 gtools_MANS)
SET(gtools_ignore_files
	beset/beset.h
	beset/fitness.h
	beset/population.h
	)
CMAKEFILES(${gtools_ignore_files})
CMAKEFILES(Makefile.am beset/Makefile.am)
