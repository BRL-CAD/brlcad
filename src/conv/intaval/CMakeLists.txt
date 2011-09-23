SET(INTAVAL_INCLUDE_DIRS
	${BU_INCLUDE_DIRS}
	${RT_INCLUDE_DIRS}
	${WDB_INCLUDE_DIRS}
	)
LIST(REMOVE_DUPLICATES INTAVAL_INCLUDE_DIRS)
include_directories(${INTAVAL_INCLUDE_DIRS})

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
CMAKEFILES(Makefile.am)
