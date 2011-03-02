include_directories(
	${ZLIB_INCLUDE_DIR}
	${OPENNURBS_INCLUDE_DIR}
	${REGEX_INCLUDE_DIR}
	${TCL_INCLUDE_DIRS}
)

IF(MSVC)
add_definitions(
    -DON_DLL_IMPORTS
)
ENDIF(MSVC)

SET(tgf_SRCS
	 read_dra.cpp
    regtab.cpp
    tgf-g.cpp
    write_brl.cpp
)
BRLCAD_ADDEXEC(tgf-g "${tgf_SRCS}" "libwdb librt libbu")

SET(intaval_ignore_files
	glob.h
	read_dra.h
	readme.txt
	regtab.h
	write_brl.h
	)
CMAKEFILES(${intaval_ignore_files})
