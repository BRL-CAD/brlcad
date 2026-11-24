file(MAKE_DIRECTORY "${WORK}")
set(input "${WORK}/hierarchy.FCStd")
set(output "${WORK}/hierarchy.g")
file(REMOVE "${output}")

function(pack_fixture output directory)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${output}" --format=zip ${ARGN}
    WORKING_DIRECTORY "${directory}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()

execute_process(COMMAND "${FIXTURE}" generate
  WORKING_DIRECTORY "${WORK}" COMMAND_ERROR_IS_FATAL ANY)
# Put shape members ahead of metadata to exercise archive order independence.
pack_fixture("${input}" "${WORK}" box.brp GuiDocument.xml cylinder.bin torus.brp periodic.brp Document.xml)
execute_process(COMMAND "${GCV}" -i "${input}" -o "${output}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${FIXTURE}" check "${output}" COMMAND_ERROR_IS_FATAL ANY)

function(check_rejected name xml error_message)
  set(input "${WORK}/${name}.FCStd")
  set(output "${WORK}/${name}.g")
  set(members "${WORK}/${name}")
  file(REMOVE "${output}")
  file(WRITE "${members}/Document.xml" "${xml}")
  pack_fixture("${input}" "${members}" Document.xml)
  execute_process(COMMAND "${GCV}" -i "${input}" -o "${output}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(NOT status MATCHES "^[1-9][0-9]*$" OR EXISTS "${output}" OR
      NOT "${stdout}${stderr}" MATCHES "${error_message}")
    message(FATAL_ERROR "FreeCAD fixture ${name} was not rejected cleanly\n${stdout}${stderr}")
  endif()
endfunction()

check_rejected(truncated "<Document><ObjectData>" "malformed FCStd XML")
check_rejected(doctype "<!DOCTYPE Document><Document/>" "unexpected document type in FCStd XML")
check_rejected(duplicate [=[<Document><Objects><Object name="Part" type="Part::Feature"/>
  <Object name="Part" type="Part::Feature"/></Objects></Document>]=] "duplicate object in FCStd document")
