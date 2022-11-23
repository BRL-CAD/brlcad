add_definitions(
  -D_CONSOLE
  -DBRLCAD_DLL
  )

set(ASC_CONV_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(ASC_CONV_INCLUDE_DIRS)

BRLCAD_ADDEXEC(asc2dsp asc2dsp.c libbu FOLDER Conv)

BRLCAD_ADDEXEC(asc2pix asc2pix.c libbu FOLDER Conv)

BRLCAD_ADDEXEC(pix2asc pix2asc.c libbu FOLDER Conv)

if (TARGET libtclcad)

  set(ASC_CONV_INCLUDE_DIRS
    ${BN_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${GED_INCLUDE_DIRS}
    ${GCV_INCLUDE_DIRS}
    ${SYSV_INCLUDE_DIRS}
    ${WDB_INCLUDE_DIRS}
    ${TCLCAD_INCLUDE_DIRS}
    ${REGEX_INCLUDE_DIR}
    )
  BRLCAD_INCLUDE_DIRS(ASC_CONV_INCLUDE_DIRS)

  BRLCAD_ADDEXEC(asc2g asc2g.c "libtclcad;libged;libwdb;librt;libnmg;libbu;${WINSOCK_LIB};${M_LIBRARY}" FOLDER Conv)
  BRLCAD_ADDEXEC(g2asc g2asc.c "libtclcad;librt;libnmg;libbu;${WINSOCK_LIB}" FOLDER Conv)

endif (TARGET libtclcad)

CMAKEFILES(
  CMakeLists.txt
  asc2g.c
  g2asc.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
