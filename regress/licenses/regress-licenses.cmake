set(LICENSE_CHECK_EXEC "${LCHECK_EXEC}")
set(WORKING_DIR "${W_DIR}")
set(FILES_LIST "${F_LIST}")
set(LICENSE_FILE_SET "${L_LIST}")
set(LOG_FILE "${L_FILE}")
set(STAMP_FILE "${S_FILE}")

file(WRITE "${LOG_FILE}" "Running license check:\n${LICENSE_CHECK_EXEC} ${LICENSE_FILE_SET} ${FILES_LIST}\n")
message("Processing...")
execute_process(
  COMMAND "${LICENSE_CHECK_EXEC}" "${LICENSE_FILE_SET}" "${FILES_LIST}" "${BRLCAD_SOURCE_DIR}" RESULT_VARIABLE license_result
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
