set(TERMIO_SRCS
  termio.c
  termcap.c
  tgoto.c
  tputs.c
  )

if (NOT WIN32)
  # Include directories needed by libtermio users
  set(TERMIO_INCLUDE_DIRS
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    )
  BRLCAD_LIB_INCLUDE_DIRS(termio TERMIO_INCLUDE_DIRS "")

   add_definitions(-DCM_N -DCM_GT -DCM_B -DCM_D
     -DB_TERMCAP=\"${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/termio/termcap\"
    )

  BRLCAD_ADDDATA(termcap termio)

  BRLCAD_ADDLIB(libtermio "${TERMIO_SRCS}" "libbu")
  set_target_properties(libtermio PROPERTIES VERSION 20.0.1 SOVERSION 20)
endif(NOT WIN32)

CMAKEFILES(
  ${TERMIO_SRCS}
  CMakeLists.txt
  termcap.h
  termcap
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
