if(NOT DEFINED FBHELP)
  message(FATAL_ERROR "FBHELP is required")
endif()

execute_process(
  COMMAND "${FBHELP}" -F /dev/null
  OUTPUT_VARIABLE help_output
  ERROR_VARIABLE help_error
  RESULT_VARIABLE help_result
)

if(NOT help_result EQUAL 0)
  message(FATAL_ERROR "fbhelp failed:\n${help_error}")
endif()
if(NOT help_error STREQUAL "")
  message(FATAL_ERROR "fbhelp split successful output across stderr:\n${help_error}")
endif()
if(NOT help_output MATCHES "^A Frame Buffer display device")
  message(FATAL_ERROR "fbhelp output sections are out of order:\n${help_output}")
endif()
foreach(expected "Available Devices" "Current Selection" "Description:")
  if(NOT help_output MATCHES "${expected}")
    message(FATAL_ERROR "fbhelp output is missing '${expected}':\n${help_output}")
  endif()
endforeach()
