if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT_A OR NOT DEFINED OUTPUT_B)
  message(FATAL_ERROR "AP214_G, INPUT, OUTPUT_A, and OUTPUT_B are required")
endif()

file(REMOVE "${OUTPUT_A}" "${OUTPUT_B}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env MALLOC_PERTURB_=17 "${AP214_G}" -O "${OUTPUT_A}" "${INPUT}"
  RESULT_VARIABLE result_a
  OUTPUT_VARIABLE stdout_a
  ERROR_VARIABLE stderr_a
)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env MALLOC_PERTURB_=203 "${AP214_G}" -O "${OUTPUT_B}" "${INPUT}"
  RESULT_VARIABLE result_b
  OUTPUT_VARIABLE stdout_b
  ERROR_VARIABLE stderr_b
)
if(NOT result_a EQUAL 0 OR NOT result_b EQUAL 0)
  message(FATAL_ERROR
    "determinism imports failed (${result_a}, ${result_b})\n"
    "${stdout_a}${stderr_a}${stdout_b}${stderr_b}")
endif()

file(SHA256 "${OUTPUT_A}" hash_a)
file(SHA256 "${OUTPUT_B}" hash_b)
if(NOT hash_a STREQUAL hash_b)
  message(FATAL_ERROR "identical AP214 imports are not byte-for-byte deterministic: ${hash_a} != ${hash_b}")
endif()
