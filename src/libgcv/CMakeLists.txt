# Include directories needed by libgcv users
SET(GCV_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	${RT_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(GCV_INCLUDE_DIRS)

set(LIBGCV_SOURCES
    bottess.c
    region_end.c
    region_end_mc.c
)

BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt libbu")
SET_TARGET_PROPERTIES(libgcv PROPERTIES VERSION 19.0.1 SOVERSION 19)

ADD_SUBDIRECTORY(wfobj)

CMAKEFILES(NOTES)
CMAKEFILES(Makefile.am)
