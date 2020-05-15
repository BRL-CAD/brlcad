string(REPLACE "\\" "" LICENSE_CHECK_EXEC "${LCHECK_EXEC}")
string(REPLACE "\\" "" FILES_LIST "${F_LIST}")
string(REPLACE "\\" "" LICENSE_FILE_SET "${L_LIST}")
string(REPLACE "\\" "" B_SOURCE_DIR "${BRLCAD_SOURCE_DIR}")

message("Running license check:\n${LICENSE_CHECK_EXEC} ${LICENSE_FILE_SET} ${FILES_LIST}\n")
message("Processing...")
execute_process(
  COMMAND "${LICENSE_CHECK_EXEC}" "${LICENSE_FILE_SET}" "${FILES_LIST}" "${B_SOURCE_DIR}" RESULT_VARIABLE license_result
  OUTPUT_VARIABLE license_log ERROR_VARIABLE license_log
  )
message("Processing... done.")

if(license_result)
  message("${license_log}\n")
  message(FATAL_ERROR "[license check] failed.\n")
endif(license_result)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
