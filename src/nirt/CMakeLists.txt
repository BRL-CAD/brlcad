include_directories(
    ${ZLIB_INCLUDE_DIR}
    ${OPENNURBS_INCLUDE_DIR}
	 ${REGEX_INCLUDE_DIR}
    ${TCL_INCLUDE_DIRS}
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

BRLCAD_ADDEXEC(nirt "${NIRT_SRCS}" "librt libfb")
SET_TARGET_PROPERTIES(nirt PROPERTIES COMPILE_DEFINITIONS "DFLT_SHELL=\"/bin/sh\"")

BRLCAD_ADDEXEC(showshot showshot.c libbn)
install(FILES showshot.1 DESTINATION ${MAN_DIR}/man1)

SET(nirt_sfiles
  sfiles/csv.nrt 
  sfiles/csv-gap.nrt 
  sfiles/default.nrt 
  sfiles/entryexit.nrt 
  sfiles/gap1.nrt 
  sfiles/gap2.nrt
)
FILE(COPY ${nirt_sfiles} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/nirt)
INSTALL(FILES ${nirt_sfiles} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/nirt)
