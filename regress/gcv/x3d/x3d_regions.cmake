# Exporting several regions must release each region only once, including
# when a separate light-source walk precedes the geometry walk.

file(MAKE_DIRECTORY "${TEST_DIR}")
set(regions platform box cone ellipse tor)

foreach(selection IN ITEMS group list)
  set(output "${TEST_DIR}/regions-${selection}.x3d")
  file(REMOVE "${output}")
  if(selection STREQUAL "group")
    set(objects all.g)
    set(prefix "/all_g")
  else()
    set(objects)
    foreach(region IN LISTS regions)
      list(APPEND objects "${region}.r")
    endforeach()
    set(prefix "")
  endif()

  execute_process(
    COMMAND "${X3D_EXEC}" -o "${output}" "${INPUT}" ${objects}
    WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result STREQUAL "0")
    message(FATAL_ERROR "g-x3d ${selection} failed: ${export_result}\n${export_output}\n${export_error}")
  endif()

  file(READ "${output}" contents)
  foreach(region IN LISTS regions)
    string(FIND "${contents}" "<Shape DEF=\"${prefix}/${region}_r\">" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "g-x3d ${selection} omitted region ${region}.r")
    endif()
  endforeach()
  string(FIND "${contents}" "</X3D>" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "g-x3d ${selection} did not finish the output document")
  endif()
endforeach()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
