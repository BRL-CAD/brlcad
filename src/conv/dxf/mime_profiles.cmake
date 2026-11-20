# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
# See the file named COPYING for more information.

if(NOT DEFINED WORK_DIR OR NOT DEFINED MGED OR NOT DEFINED G_DXF OR
   NOT DEFINED DXF_G OR NOT DEFINED GCV)
  message(FATAL_ERROR "WORK_DIR, MGED, G_DXF, DXF_G and GCV are required")
endif()
file(MAKE_DIRECTORY "${WORK_DIR}")
set(database "${WORK_DIR}/source.g")
set(drawing "${WORK_DIR}/source.dxf")
set(native "${WORK_DIR}/native.g")
set(converted "${WORK_DIR}/converted.g")
set(bom_drawing "${WORK_DIR}/bom.dxf")
set(bom_converted "${WORK_DIR}/bom.g")
set(preserved "${WORK_DIR}/preserved.g")
file(REMOVE "${database}" "${drawing}" "${native}" "${converted}" "${bom_drawing}" "${bom_converted}")

function(convert)
  execute_process(COMMAND ${ARGV}
    RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "DXF MIME conversion failed (${status}): ${stdout}${stderr}")
  endif()
endfunction()

convert("${MGED}" -c "${database}" "in box.s rpp 0 10 0 20 0 30
r box.r u box.s")
convert("${G_DXF}" -o "${drawing}" "${database}" box.r)
execute_process(COMMAND "${GCV}" --identify "${drawing}"
  RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
set(identity "${stdout}${stderr}")
if(NOT status EQUAL 0 OR NOT identity MATCHES "image/vnd.dxf" OR
   NOT identity MATCHES "dxf.serialization=text" OR identity MATCHES "dxf.acad-version=")
  message(FATAL_ERROR "Native DXF identity or header observations are incorrect: ${identity}")
endif()
convert("${GCV}" --input-handler "Assetimport Reader" --input-format image/vnd.dxf
  --input-trait dxf.serialization=text "${drawing}" "${converted}")
convert("${DXF_G}" "${drawing}" "${native}")
string(ASCII 239 187 191 bom)
file(READ "${drawing}" text)
file(WRITE "${bom_drawing}" "${bom}${text}")
convert("${GCV}" --input-handler "Assetimport Reader" --input-format image/vnd.dxf
  --input-trait encoding=utf-8 "${bom_drawing}" "${bom_converted}")
foreach(output "${converted}" "${native}" "${bom_converted}")
  if(NOT EXISTS "${output}")
    message(FATAL_ERROR "DXF reader did not publish its database: ${output}")
  endif()
  file(SIZE "${output}" size)
  if(size EQUAL 0)
    message(FATAL_ERROR "DXF reader published an empty file: ${output}")
  endif()
  execute_process(COMMAND "${MGED}" -c "${output}" "ls -l -p"
    RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
  if(NOT status EQUAL 0 OR NOT "${stdout}${stderr}" MATCHES "[ \t]bot[ \t]")
    message(FATAL_ERROR "DXF reader did not publish readable BoT geometry: ${stdout}${stderr}")
  endif()
endforeach()

set(marker "preserve destination\n")
file(WRITE "${preserved}" "${marker}")
execute_process(COMMAND "${GCV}" --input-handler "Assetimport Reader" --input-format image/vnd.dxf
  --input-trait dxf.acad-version=AC1027 "${drawing}" "${preserved}"
  RESULT_VARIABLE status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
file(READ "${preserved}" actual)
if(NOT status EQUAL 1 OR NOT actual STREQUAL marker OR
   NOT "${stdout}${stderr}" MATCHES "SDK cannot convert the declared")
  message(FATAL_ERROR "Missing DXF revision assertion changed the destination: ${stdout}${stderr}")
endif()
