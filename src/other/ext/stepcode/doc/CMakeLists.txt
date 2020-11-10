SET(sc_MANS
    man/man1/dataprobe.1
    man/man1/fedex.1
    man/man1/exp2cxx.1
    man/man1/mkProbe.1
)

if(NOT DEFINED MAN_DIR)
  set(MAN_DIR share/man)
endif(NOT DEFINED MAN_DIR)

IF(NOT WIN32)
  INSTALL(FILES ${sc_MANS} DESTINATION ${MAN_DIR}/man1)
ENDIF(NOT WIN32)
