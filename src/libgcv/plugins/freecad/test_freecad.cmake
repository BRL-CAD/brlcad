file(MAKE_DIRECTORY "${WORK}")

function(pack_fixture output directory)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${output}" --format=zip ${ARGN}
    WORKING_DIRECTORY "${directory}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(check_failed input output error_message)
  file(REMOVE "${output}")
  execute_process(COMMAND "${GCV}" ${ARGN} -i "${input}" -o "${output}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(NOT status MATCHES "^[1-9][0-9]*$" OR EXISTS "${output}" OR
      NOT "${stdout}${stderr}" MATCHES "${error_message}")
    message(FATAL_ERROR "FreeCAD fixture ${input} was not rejected cleanly\n${stdout}${stderr}")
  endif()
endfunction()

if(CASE STREQUAL "gcv_freecad_arrays")
  execute_process(COMMAND "${FIXTURE}" generate-arrays
    WORKING_DIRECTORY "${WORK}" COMMAND_ERROR_IS_FATAL ANY)
  set(input "${WORK}/arrays.FCStd")
  set(output "${WORK}/arrays.g")
  file(REMOVE "${output}")
  pack_fixture("${input}" "${WORK}" placements.bin box.brp Document.xml scales.bin)
  execute_process(COMMAND "${GCV}" -I --strict -i "${input}" -o "${output}" COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${FIXTURE}" check-arrays "${output}" COMMAND_ERROR_IS_FATAL ANY)

  set(input "${WORK}/missing-array.FCStd")
  set(output "${WORK}/missing-array.g")
  pack_fixture("${input}" "${WORK}" box.brp Document.xml scales.bin)
  check_failed("${input}" "${output}" "missing an array placement or scale member" -I --strict)
  execute_process(COMMAND "${GCV}" -i "${input}" -o "${output}"
    COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  if(NOT EXISTS "${output}" OR NOT "${stdout}${stderr}" MATCHES "partial import")
    message(FATAL_ERROR "FreeCAD default import did not report its partial result\n${stdout}${stderr}")
  endif()

  file(WRITE "${WORK}/placements.bin" "bad")
  set(input "${WORK}/malformed-array.FCStd")
  pack_fixture("${input}" "${WORK}" box.brp Document.xml scales.bin placements.bin)
  check_failed("${input}" "${WORK}/malformed-array.g" "truncated FCStd link array header")
  return()
endif()

set(input "${WORK}/hierarchy.FCStd")
set(output "${WORK}/hierarchy.g")
file(REMOVE "${output}")
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
  file(WRITE "${members}/Document.xml" "${xml}")
  pack_fixture("${input}" "${members}" Document.xml)
  check_failed("${input}" "${output}" "${error_message}")
endfunction()

check_rejected(truncated "<Document><ObjectData>" "malformed FCStd XML")
check_rejected(doctype "<!DOCTYPE Document><Document/>" "unexpected document type in FCStd XML")
check_rejected(duplicate [=[<Document><Objects><Object name="Part" type="Part::Feature"/>
  <Object name="Part" type="Part::Feature"/></Objects></Document>]=] "duplicate object in FCStd document")

set(partial "${WORK}/partial")
file(MAKE_DIRECTORY "${partial}")
file(COPY_FILE "${WORK}/box.brp" "${partial}/box.brp")
file(WRITE "${partial}/Document.xml" [=[
<Document>
  <Objects><Object name="Box" type="Part::Feature"/><Object name="Remote" type="App::Link"/></Objects>
  <ObjectData>
    <Object name="Box"><Properties><Property name="Shape"><Part file="box.brp"/></Property></Properties></Object>
    <Object name="Remote"><Properties>
      <Property name="LinkedObject"><XLink file="external.FCStd" name="Box"/></Property>
    </Properties></Object>
  </ObjectData>
</Document>
]=])
pack_fixture("${WORK}/partial.FCStd" "${partial}" Document.xml box.brp)
check_failed("${WORK}/partial.FCStd" "${WORK}/partial.g" "strict import rejected" -I --strict)
