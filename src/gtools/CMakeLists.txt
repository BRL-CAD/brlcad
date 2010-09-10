include_directories(
  ${BRLCAD_STD_INCLUDES}
  ${TCL_INCLUDE_PATH}
  ${BRLCAD_SOURCE_DIR}/src/gtools/beset
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

SET(beset_SRCS
  beset/beset.c
  beset/fitness.c
  beset/population.c
)

BRLCAD_ADDEXEC(beset "${beset_SRCS}" wdb)
BRLCAD_ADDEXEC(g_diff g_diff.c "librt ged tclcad")
BRLCAD_ADDEXEC(g_lint g_lint.c librt)
BRLCAD_ADDEXEC(g_qa g_qa.c "librt ged")
BRLCAD_ADDEXEC(remapid remapid.c "bu librt")

add_executable(g_transfer g_transfer.c)
target_link_libraries(g_transfer librt pkg)

INSTALL(FILES g_transfer.c DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/sample_applications)

SET(gtools_MANS
  g_diff.1
  g_lint.1
  remapid.1
)

install(FILES ${gtools_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
