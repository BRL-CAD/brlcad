include_directories(
  ${BRLCAD_BINARY_DIR}/include/brlcad
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
)

IF(CRYPT_LIBRARY)
	add_executable(enigma enigma.c)
	target_link_libraries(enigma ${BSD_LIBRARY} ${CRYPT_LIBRARY})
ELSE(CRYPT_LIBRARY)
	add_executable(enigma enigma.c crypt.c)
	target_link_libraries(enigma ${BSD_LIBRARY})
ENDIF(CRYPT_LIBRARY)
install(TARGETS enigma DESTINATION ${BIN_DIR})
set_target_properties(enigma PROPERTIES COMPILE_FLAGS "-w")

ADD_MAN_PAGE(1 enigma.1)
SET(enigma_ignore_files
	AUTHORS
	COPYING
	ChangeLog
	INSTALL
	NEWS
	README
	configure.ac
	crypt.c
	Makefile.am
)
CMAKEFILES(${enigma_ignore_files})
