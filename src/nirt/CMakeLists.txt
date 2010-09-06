include_directories(
    ${BRLCAD_STD_INCLUDES}
    ${BRLCAD_ZLIB_INCLUDE_DIR}
    ${BRLCAD_OPENNURBS_INCLUDE_DIR}
    ${BRLCAD_REGEX_INCLUDE_DIR}
    ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

SET(NIRT_SRCS
    nirt.c
    command.c
    conversion.c
    if.c
    interact.c
    bsphere.c
    read_mat.c
    str_manip.c
    parse_fmt.c
)

BRLCAD_ADDEXEC(nirt "${NIRT_SRCS}" "fb librt")
SET_TARGET_PROPERTIES(nirt PROPERTIES COMPILE_DEFINITIONS "DFLT_SHELL=\"/bin/sh\"")

BRLCAD_ADDEXEC(showshot showshot.c bn)
install(FILES showshot.1 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
