set(Y2038_SOURCES
  time64.c
  )

if(MSVC)
  add_definitions("-DY2038_DLL_EXPORTS")
endif(MSVC)

BRLCAD_ADDLIB(y2038 "${Y2038_SOURCES}" "")

set(Y2038_FILES
  LICENSE
  README.txt
  time64.h
  time64_config.h
  time64_limits.h
  )

CMAKEFILES(${Y2038_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

