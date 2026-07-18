if(NOT DEFINED SUMMARY)
  message(FATAL_ERROR "SUMMARY is required")
endif()
file(REMOVE "${REPORT}" "${SUMMARY}")
execute_process(
  COMMAND "${AP214_G}" -D -S "${SUMMARY}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "ap214-g dry run failed (${result}):\n${output}\n${error}")
endif()

if(NOT EXISTS "${SUMMARY}")
  message(FATAL_ERROR "ap214-g dry run did not write its legacy summary")
endif()
file(READ "${SUMMARY}" summary_text)
if(NOT summary_text MATCHES "156,DESCRIPTIVE_REPRESENTATION_ITEM,NOT_PROCESSED")
  message(FATAL_ERROR "lazy legacy summary omitted the unmaterialized descriptive item:\n${summary_text}")
endif()

if(NOT EXISTS "${REPORT}")
  message(FATAL_ERROR "ap214-g dry run did not write its report")
endif()

file(READ "${REPORT}" report_text)
if(NOT report_text MATCHES "\"dry_run\":true")
  message(FATAL_ERROR "dry-run report does not record dry-run mode")
endif()
if(NOT report_text MATCHES "\"output\":\"\"")
  message(FATAL_ERROR "dry-run report unexpectedly names an output database")
endif()
foreach(expected
    "\"loaded_instances\":0"
    "\"pinned_instances\":0"
    "\"active_batches\":0"
    "\"bytes\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "dry-run cache report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
if(NOT report_text MATCHES "\"high_water_instances\":[1-9][0-9]*" OR
   NOT report_text MATCHES "\"high_water_bytes\":[1-9][0-9]*" OR
   NOT report_text MATCHES "\"materializations\":[1-9][0-9]*")
  message(FATAL_ERROR "dry-run cache report lacks nonzero high-water/materialization counters:\n${report_text}")
endif()
