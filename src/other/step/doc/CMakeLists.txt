SET(scl_MANS
	man/man1/dataprobe.1 
	man/man1/fedex.1 
	man/man1/fedex_plus.1 
	man/man1/mkProbe.1
)
INSTALL(FILES ${scl_MANS} DESTINATION ${MAN_DIR}/man1)
