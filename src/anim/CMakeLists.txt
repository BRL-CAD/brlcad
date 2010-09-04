include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD=1
)

# Flesh this out and make it standard?  Add a regex to allow
# srcs and libs to come in using spaces and get converted
# to lists?  Would work if we have no filenames/paths with spaces
MACRO(BRLCAD_ADDEXEC execname srcs libs)
  add_executable(${execname} ${srcs})
  target_link_libraries(${execname} ${libs})
  INSTALL(TARGETS ${execname} RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
ENDMACRO(BRLCAD_ADDEXEC execname srcs libs)

BRLCAD_ADDEXEC(anim_cascade anim_cascade.c bn)

BRLCAD_ADDEXEC(anim_fly anim_fly.c bu)

BRLCAD_ADDEXEC(anim_hardtrack anim_hardtrack.c bn)

BRLCAD_ADDEXEC(anim_keyread anim_keyread.c bn)

BRLCAD_ADDEXEC(anim_lookat anim_lookat.c bn)

BRLCAD_ADDEXEC(anim_offset anim_offset.c bn)

BRLCAD_ADDEXEC(anim_orient anim_orient.c bn)

BRLCAD_ADDEXEC(anim_script anim_script.c bn)

BRLCAD_ADDEXEC(anim_sort anim_sort.c bu)

BRLCAD_ADDEXEC(anim_time anim_time.c bu)

BRLCAD_ADDEXEC(anim_track "anim_track.c;cattrack.c" bn)

BRLCAD_ADDEXEC(anim_turn anim_turn.c bn)

BRLCAD_ADDEXEC(chan_add chan_add.c bu)

BRLCAD_ADDEXEC(chan_mult chan_mult.c bu)

BRLCAD_ADDEXEC(chan_permute chan_permute.c bu)
