if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, INPUT, and OUTPUT are required")
endif()

file(REMOVE "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -e 91 "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
  TIMEOUT 60
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "step-g positional AP203 import failed (${result})\n${stdout}\n${stderr}")
endif()
if(NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR "step-g did not create its positional output")
endif()
string(FIND "${stdout}${stderr}" "STEP lazy cache: indexed=" lazy_summary)
if(lazy_summary EQUAL -1)
  message(FATAL_ERROR "step-g did not report use of the lazy index\n${stdout}\n${stderr}")
endif()
string(FIND "${stdout}${stderr}" "current-loaded=0, current-pinned=0" released_summary)
if(released_summary EQUAL -1)
  message(FATAL_ERROR "the final AP203 dependency batch was not fully released\n${stdout}\n${stderr}")
endif()
