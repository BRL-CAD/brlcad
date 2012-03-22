SET(scl_MANS
	man/man1/dataprobe.1 
	man/man1/fedex.1 
	man/man1/fedex_plus.1 
	man/man1/mkProbe.1
)
IF(NOT WIN32)
  INSTALL(FILES ${scl_MANS} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
ENDIF(NOT WIN32)