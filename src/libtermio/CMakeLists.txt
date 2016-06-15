if(HAVE_TERMLIB)
  # Include directories needed by libtermio users
  set(TERMIO_INCLUDE_DIRS
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${TERMLIB_INCLUDE_DIRS}
    )
  BRLCAD_LIB_INCLUDE_DIRS(termio TERMIO_INCLUDE_DIRS "")

  BRLCAD_ADDLIB(libtermio termio.c "${TERMLIB_LIBRARY}")
  SET_TARGET_PROPERTIES(libtermio PROPERTIES VERSION 20.0.1 SOVERSION 20)
endif(HAVE_TERMLIB)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
