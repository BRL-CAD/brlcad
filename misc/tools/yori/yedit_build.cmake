execute_process(
	COMMAND nmake
	WORKING_DIRECTORY ${YORI_SRC_DIR}/crt
)
execute_process(
	COMMAND nmake
	WORKING_DIRECTORY ${YORI_SRC_DIR}/lib
)
execute_process(
	COMMAND nmake
	WORKING_DIRECTORY ${YORI_SRC_DIR}/libwin
)
execute_process(
	COMMAND nmake
	WORKING_DIRECTORY ${YORI_SRC_DIR}/libdlg
)
execute_process(
	COMMAND nmake
	WORKING_DIRECTORY ${YORI_SRC_DIR}/edit
)

