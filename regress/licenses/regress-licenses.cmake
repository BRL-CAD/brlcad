string(REPLACE "\\" "" LICENSE_CHECK_EXEC "${LCHECK_EXEC}")
string(REPLACE "\\" "" WORKING_DIR "${W_DIR}")
string(REPLACE "\\" "" FILES_LIST "${F_LIST}")
string(REPLACE "\\" "" LICENSE_FILE_SET "${L_LIST}")
string(REPLACE "\\" "" LOG_FILE "${L_FILE}")
string(REPLACE "\\" "" STAMP_FILE "${S_FILE}")
string(REPLACE "\\" "" B_SOURCE_DIR "${BRLCAD_SOURCE_DIR}")

file(WRITE "${LOG_FILE}" "Running license check:\n${LICENSE_CHECK_EXEC} ${LICENSE_FILE_SET} ${FILES_LIST}\n")
message("Processing...")
execute_process(
  COMMAND "${LICENSE_CHECK_EXEC}" "${LICENSE_FILE_SET}" "${FILES_LIST}" "${B_SOURCE_DIR}" RESULT_VARIABLE license_result
  OUTPUT_VARIABLE license_log ERROR_VARIABLE license_log
  WORKING_DIRECTORY ${WORKING_DIR}
  )
message("Processing... done.")

file(APPEND "${LOG_FILE}" "\n${license_log}\n")

if(license_result)
  file(APPEND "${LOG_FILE}" "\n\nError: return code ${license_result}")
  message(FATAL_ERROR "[license check] Failure, see ${LOG_FILE} for more info.\n")
else(license_result)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${STAMP_FILE}")
endif(license_result)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
