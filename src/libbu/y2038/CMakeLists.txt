set(Y2038_SOURCES
  time64.c
  )

if(MSVC)
	add_definitions("-DY2038_DLL_EXPORTS")
endif(MSVC)

BRLCAD_ADDLIB(y2038 "${Y2038_SOURCES}" "")
