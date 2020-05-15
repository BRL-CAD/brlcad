string(REPLACE "\\" "" REPOSITORY_CHECK_EXEC "${RCHECK_EXEC}")
string(REPLACE "\\" "" FILES_LIST "${F_LIST}")
string(REPLACE "\\" "" B_SOURCE_DIR "${B_DIR}")

message("Running repository check:\n${REPOSITORY_CHECK_EXEC} ${REPOSITORY_FILE_SET} ${FILES_LIST} \"${B_SOURCE_DIR}\"\n")
message("Processing...")
execute_process(
  COMMAND "${REPOSITORY_CHECK_EXEC}" "${FILES_LIST}" "${B_SOURCE_DIR}" RESULT_VARIABLE repository_result
  OUTPUT_VARIABLE repository_log ERROR_VARIABLE repository_log
  )
message("Processing... done.")

if(repository_result)
  message("${repository_log}\n")
  message(FATAL_ERROR "[repository check] failed.\n")
endif(repository_result)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
