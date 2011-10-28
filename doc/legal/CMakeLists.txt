set(doc_legal
	bsd.txt
	bdl.txt
	lgpl.txt
)

install(FILES ${doc_legal} DESTINATION ${DATA_DIR}/doc/legal)

CMAKEFILES(Makefile.am bsd.txt bdl.txt lgpl.txt)
