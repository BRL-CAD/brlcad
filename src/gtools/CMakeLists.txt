include_directories(
  ${TCL_INCLUDE_DIRS}
  ${BRLCAD_SOURCE_DIR}/src/gtools/beset
)

SET(beset_SRCS
  beset/beset.c
  beset/fitness.c
  beset/population.c
)

add_executable(beset ${beset_SRCS})
target_link_libraries(beset libwdb)

BRLCAD_ADDEXEC(g_diff g_diff.c "libtclcad libged librt")
BRLCAD_ADDEXEC(g_lint g_lint.c librt)
BRLCAD_ADDEXEC(g_qa g_qa.c "libged librt")
BRLCAD_ADDEXEC(remapid remapid.c "librt libbu")

add_executable(g_transfer g_transfer.c)
target_link_libraries(g_transfer librt libpkg)

INSTALL(FILES g_transfer.c DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/sample_applications)

SET(gtools_MANS
  g_diff.1
  g_lint.1
  remapid.1
)

install(FILES ${gtools_MANS} DESTINATION ${MAN_DIR}/man1)
