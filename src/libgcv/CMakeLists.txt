set(LIBGCV_SOURCES
    bottess.c
    region_end.c
    region_end_mc.c
)

include_directories(
    ${REGEX_INCLUDE_DIR}
    ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt libbu")
SET_TARGET_PROPERTIES(libgcv PROPERTIES VERSION 19.0.1 SOVERSION 19)

CMAKEFILES(NOTES wfobj)
