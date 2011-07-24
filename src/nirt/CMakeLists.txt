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

BRLCAD_ADDEXEC(nirt "${NIRT_SRCS}" "librt libfb ${M_LIBRARY}")
SET_TARGET_PROPERTIES(nirt PROPERTIES COMPILE_DEFINITIONS "DFLT_SHELL=\"/bin/sh\"")

BRLCAD_ADDEXEC(showshot showshot.c libbn)
ADD_MAN_PAGE(1 showshot.1)

SET(nirt_sfiles
  sfiles/csv.nrt 
  sfiles/csv-gap.nrt 
  sfiles/default.nrt 
  sfiles/entryexit.nrt 
  sfiles/gap1.nrt 
  sfiles/gap2.nrt
)
BRLCAD_ADDDATA(nirt_sfiles nirt)

SET(nirt_ignore_files
	nirt.h
	usrfmt.h
	)
CMAKEFILES(${nirt_ignore_files})
