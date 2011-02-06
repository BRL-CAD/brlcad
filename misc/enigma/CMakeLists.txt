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
	BRLCAD_ADDEXEC(enigma enigma.c "${BSD_LIBRARY} ${CRYPT_LIBRARY}")
ELSE(CRYPT_LIBRARY)
	BRLCAD_ADDEXEC(enigma "enigma.c crypt.c" "${BSD_LIBRARY}")
ENDIF(CRYPT_LIBRARY)
ADD_MAN_PAGE(1 enigma.1)
SET(enigma_ignore_files
	AUTHORS
	COPYING
	ChangeLog
	INSTALL
	NEWS
	configure.ac
	crypt.c
)
CMAKEFILES(${enigma_ignore_files})
