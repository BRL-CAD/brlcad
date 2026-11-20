file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(COMMAND "${STEP_HOST}" --list-schemas
  RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "STEP reader schema listing failed: ${stdout}${stderr}")
endif()
string(REGEX MATCHALL "ap[0-9]+(e[0-9]+)?" schemas "${stdout}${stderr}")
execute_process(COMMAND "${STEP_HOST}" --list-formats
  RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "STEP reader MIME listing failed: ${stdout}${stderr}")
endif()
set(formats "${stdout}${stderr}")
foreach(schema IN LISTS schemas)
  if(NOT formats MATCHES "model/step read ${schema} ")
    message(FATAL_ERROR "Missing configured reader profile: ${schema}: ${formats}")
  endif()
endforeach()
if(formats MATCHES "application/p21 read")
  message(FATAL_ERROR "STEP reader discovery promised arbitrary EXPRESS input: ${formats}")
endif()
set(input "${WORK_DIR}/source.step")
set(output "${WORK_DIR}/preserved.g")
set(marker "preserve destination")
function(reject source schema diagnostic)
  file(WRITE "${input}" "${source}")
  file(WRITE "${output}" "${marker}")
  execute_process(COMMAND "${STEP_HOST}" ${ARGN} --schema "${schema}" -o "${output}" "${input}"
    RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  if(NOT status EQUAL 2 OR NOT "${stdout}${stderr}" MATCHES "${diagnostic}")
    message(FATAL_ERROR "STEP MIME rejection failed: ${status}: ${stdout}${stderr}")
  endif()
  file(READ "${output}" actual)
  if(NOT actual STREQUAL marker)
    message(FATAL_ERROR "Rejected STEP selection changed the destination")
  endif()
endfunction()
foreach(revision 1 3 4 5)
  reject("ISO-10303-21; HEADER; FILE_SCHEMA(('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 ${revision} 1 4}')); ENDSEC; DATA;"
    ap203 "AP242 input requires")
endforeach()
reject("ISO-10303-21; HEADER; FILE_SCHEMA(('CONFIG_CONTROL_DESIGN','SHAPE_APPEARANCE_LAYER_MIM {1 2 3}')); ENDSEC; DATA;"
  ap203 "FILE_SCHEMA selected ap203e2")
reject("ISO-10303-21; HEADER; FILE_DESCRIPTION(('ENDSEC; FILE_SCHEMA((''AUTOMOTIVE_DESIGN''));'),'2;1'); FILE_SCHEMA(('CONFIG_CONTROL_DESIGN')); ENDSEC; DATA;"
  ap242e4 "FILE_SCHEMA selected ap203")
reject("ISO-10303-21; HEADER; FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'); ENDSEC; DATA;"
  auto "FILE_SCHEMA declaration")
reject("ISO-10303-21; HEADER; FILE_SCHEMA(('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 1 1 4}', 'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 5 1 4}')); ENDSEC; DATA;"
  auto "ambiguous mixed STEP editions")
file(WRITE "${input}" "ISO-10303-21; HEADER; FILE_SCHEMA(('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 1 1 4}')); ENDSEC; DATA;")
execute_process(COMMAND "${STEP_HOST}" --schema ap203 -D - INPUT_FILE "${input}"
  RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT status EQUAL 2 OR NOT "${stdout}${stderr}" MATCHES "AP242 input requires")
  message(FATAL_ERROR "STEP stdin selection failed: ${status}: ${stdout}${stderr}")
endif()

set(ap242 AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF)
set(known "ISO-10303-21; HEADER; FILE_SCHEMA(('${ap242} {1 0 10303 442 1 1 4}')); ENDSEC; DATA;")
reject("${known}" ap242e4 "does not establish requested MIME traits"
  --input-trait step.edition=4)
reject("${known}" ap203 "AP242 input requires"
  --input-format model/step --input-trait step.ap=242 --input-trait step.edition=1)
reject("${known}" ap203 "AP242 input requires"
  --input-format application/p21 --input-trait "p21.schema=${ap242}")
reject("${known}" auto "requested MIME format does not match"
  --input-format image/png)
reject("${known}" auto "does not establish requested MIME traits"
  --input-trait version=1)
reject("${known}" auto "does not establish requested MIME traits"
  --input-trait [=[step.application-protocols[]=["203","242"]]=])
reject("ISO-10303-21; HEADER; FILE_SCHEMA(('${ap242} {1 0 10303 442 999 1 4}')); ENDSEC; DATA;"
  auto "does not establish requested MIME traits" --input-trait step.edition=1)
reject("ISO-10303-21; HEADER; FILE_SCHEMA(('${ap242}')); ENDSEC; DATA;"
  auto "does not establish requested MIME traits" --input-trait [=[step.profiles[]=[]]=])
reject("ISO-10303-21; HEADER; FILE_SCHEMA(('CONFIG_CONTROL_DESIGN','ALIBRE_SCHEMA')); ENDSEC; DATA;"
  auto "requested MIME format does not match" --input-format model/step)
string(REPEAT "p" 70000 comment)
set(large "ISO-10303-21; HEADER; /*${comment}*/ FILE_SCHEMA(('${ap242} {1 0 10303 442 1 1 4}')); ENDSEC; DATA;")
reject("${large}" ap203 "AP242 input requires"
  --input-format model/step --input-trait step.edition=1)
reject("${large}" ap242e4 "does not establish requested MIME traits"
  --input-trait step.edition=4)
file(WRITE "${input}" "${known}")
execute_process(COMMAND "${STEP_HOST}" --schema ap242e4 --input-trait step.edition=4 -D - INPUT_FILE "${input}"
  RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT status EQUAL 2 OR NOT "${stdout}${stderr}" MATCHES "does not establish requested MIME traits")
  message(FATAL_ERROR "STEP stdin MIME assertion failed: ${status}: ${stdout}${stderr}")
endif()
