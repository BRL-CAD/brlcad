execute_process(
  COMMAND "${ICV_CODEC_TEST}" --emit
  COMMAND "${ICV_CODEC_TEST}" --stdin
  RESULTS_VARIABLE statuses
)
if(NOT "${statuses}" STREQUAL "0;0")
  message(FATAL_ERROR "Image codec pipe round-trip failed: ${statuses}")
endif()

execute_process(COMMAND "${ICV_CODEC_TEST}" --emit-unsupported
  COMMAND "${ICV_CODEC_TEST}" --reject-unsupported RESULTS_VARIABLE statuses)
if(NOT "${statuses}" STREQUAL "0;0")
  message(FATAL_ERROR "Unsupported pipe was consumed past its prefix: ${statuses}")
endif()
execute_process(COMMAND "${ICV_CODEC_TEST}" --emit
  COMMAND "${ICV_CODEC_TEST}" --stdin-limit RESULTS_VARIABLE statuses)
if(NOT "${statuses}" STREQUAL "0;0")
  message(FATAL_ERROR "Pipe buffering limit was not enforced: ${statuses}")
endif()

# Put maxval beyond the default 64 KiB prefix so only the full reader can check it.
set(ppm_comment_size 65537)
string(REPEAT "x" ${ppm_comment_size} ppm_comment)
set(ppm_input "${CMAKE_CURRENT_BINARY_DIR}/pipe-profile.ppm")
file(WRITE "${ppm_input}" "P3\n#${ppm_comment}\n1 1\n65535\n65535 0 0\n")
foreach(mode --stdin-ppm --stdin-ppm-bad)
  execute_process(COMMAND "${ICV_CODEC_TEST}" ${mode} INPUT_FILE "${ppm_input}" RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "PPM pipe profile validation failed: ${mode}: ${status}")
  endif()
endforeach()
file(REMOVE "${ppm_input}")
