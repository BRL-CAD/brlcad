string(REPLACE "\\" "" REPOSITORY_CHECK_EXEC "${RCHECK_EXEC}")
string(REPLACE "\\" "" FILES_LIST "${F_LIST}")
string(REPLACE "\\" "" LOG_FILE "${L_FILE}")
string(REPLACE "\\" "" STAMP_FILE "${S_FILE}")
string(REPLACE "\\" "" B_SOURCE_DIR "${B_DIR}")

file(WRITE "${LOG_FILE}" "Running repository check:\n${REPOSITORY_CHECK_EXEC} ${REPOSITORY_FILE_SET} ${FILES_LIST}\n")
message("Processing...")
execute_process(
  COMMAND "${REPOSITORY_CHECK_EXEC}" "${FILES_LIST}" "${B_SOURCE_DIR}" RESULT_VARIABLE repository_result
  OUTPUT_VARIABLE repository_log ERROR_VARIABLE repository_log
  )
message("Processing... done.")

file(APPEND "${LOG_FILE}" "\n${repository_log}\n")

if(repository_result)
  file(APPEND "${LOG_FILE}" "\n\nError: return code ${repository_result}")
  message(FATAL_ERROR "[repository check] Failure, see ${LOG_FILE} for more info.\n")
else(repository_result)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${STAMP_FILE}")
endif(repository_result)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
