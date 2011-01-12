set(doc_legal
	bsd.txt 
	bdl.txt
	lgpl.txt
)

install(FILES ${doc_legal} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/doc/legal)
