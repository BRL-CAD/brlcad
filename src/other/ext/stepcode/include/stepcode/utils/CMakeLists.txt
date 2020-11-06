set(UTILS_HDRS
	Str.h
	dirobj.h
	errordesc.h
	gennode.h
	gennodearray.h
	gennodelist.h
	sc_hash.h
	)

install(FILES ${UTILS_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/utils)


