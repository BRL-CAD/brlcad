
# if oneshot is true, don't run exp2cxx if source files exist. if schema.cc exists, assume others do
if(ONESHOT AND EXISTS "${SDIR}/schema.cc")
  message("WARNING: SC_GENERATE_CXX_ONESHOT is enabled. If generated code has been modified, it will NOT be rewritten!")
  message("This is ONLY for debugging STEPcode internals!")
else()
  execute_process(COMMAND ${EXE} ${EXP}
    WORKING_DIRECTORY ${SDIR}
    RESULT_VARIABLE _res
    OUTPUT_FILE exp2cxx_stdout.txt
    ERROR_FILE exp2cxx_stderr.txt
   )
  if(NOT "${_res}" STREQUAL "0")
    message(FATAL_ERROR "${EXE} reported an error for ${EXP}.\nsee exp2cxx_stdout.txt and exp2cxx_stderr.txt in ${SDIR} for details.")
  endif(NOT "${_res}" STREQUAL "0")
  # TODO count number of lines in stdout/stderr and tell user?
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

