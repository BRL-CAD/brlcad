IF(NOT MAN_DIR)
	SET(MAN_DIR "man")
ENDIF(NOT MAN_DIR)

FIND_PROGRAM(SH_EXEC NAMES sh dash bash DOC "path to shell executable")
FIND_PROGRAM(SED_EXEC NAMES sed DOC "path to sed executable")

IF(SED_EXEC AND SH_EXEC)
	SET(TK_DOC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	configure_file(install_man.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/install_man.cmake @ONLY)
	INSTALL(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/install_man.cmake)
ENDIF(SED_EXEC AND SH_EXEC)
