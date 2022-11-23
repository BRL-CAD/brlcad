set(sc_MANS
  man/man1/fedex.1
  man/man1/exp2cxx.1
  )

if(NOT DEFINED MAN_DIR)
  set(MAN_DIR share/man)
endif(NOT DEFINED MAN_DIR)

if(NOT WIN32)
  install(FILES ${sc_MANS} DESTINATION ${MAN_DIR}/man1)
endif(NOT WIN32)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

