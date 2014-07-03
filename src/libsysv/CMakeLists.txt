# Include directories needed by libsysv users
set(SYSV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${TCL_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(sysv SYSV_INCLUDE_DIRS "")

set(LIBSYSV_SOURCES
  memset.c
  strchr.c
  strdup.c
  strsep.c
  strtok.c
  tcl.c
  )

add_definitions(
  -DHAVE_MEMSET
  -DHAVE_STRCHR
  -DHAVE_STRDUP
  -DHAVE_STRTOK
  )
if(HAVE_C_WUNUSED_CONST_VARIABLE)
  add_definitions(-DHAVE_WUNUSED_CONST_VARIABLE)
endif(HAVE_C_WUNUSED_CONST_VARIABLE)

BRLCAD_ADDLIB(libsysv "${LIBSYSV_SOURCES}" "${TCL_LIBRARY}")
SET_TARGET_PROPERTIES(libsysv PROPERTIES VERSION 20.0.1 SOVERSION 20)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
