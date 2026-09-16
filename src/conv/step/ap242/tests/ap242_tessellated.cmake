if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED OPEN_INPUT OR NOT DEFINED MALFORMED_INPUT OR
   NOT DEFINED OUTPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, OPEN_INPUT, MALFORMED_INPUT, OUTPUT, and REPORT are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

function(require_command description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
  )
  if(NOT command_result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed (${command_result}):\n${command_output}${command_error}")
  endif()
  set(COMMAND_TEXT "${command_output}${command_error}" PARENT_SCOPE)
endfunction()

file(REMOVE "${OUTPUT}" "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" --strict --exact -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "exact AP242 tessellated import returned ${import_result} or omitted output:\n"
    "${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"exact\":true"
    "\"TESSELLATED_SHAPE_REPRESENTATION\":1"
    "\"TESSELLATED_SOLID\":2"
    "\"TESSELLATED_SHELL\":1"
    "\"TRIANGULATED_FACE\":5"
    "\"COMPLEX_TRIANGULATED_FACE\":1"
    "\"TRIANGULATED_SURFACE_SET\":1"
    "\"COMPLEX_TRIANGULATED_SURFACE_SET\":1"
    "\"geometry_attempted\":5,\"geometry_written\":5,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"type\":\"TESSELLATED_SOLID\",\"status\":\"handled\""
    "\"type\":\"TESSELLATED_SHELL\",\"status\":\"handled\""
    "\"type\":\"TRIANGULATED_SURFACE_SET\",\"status\":\"handled\""
    "\"type\":\"COMPLEX_TRIANGULATED_SURFACE_SET\",\"status\":\"handled\""
    "\"tolerance_mm\":0.002"
    "\"skipped_items\":[]")
  require_text("${report_text}" "${expected}" "AP242 tessellated report")
endforeach()

set(solid AP242_Tessellated_Product_tessellated_item)
set(strip AP242_Tessellated_Product_tessellated_item_step5)
set(shell AP242_Tessellated_Product_tessellated_item_step8)
set(triangle_set AP242_Tessellated_Product_tessellated_item_step19)
set(separate_solid AP242_Tessellated_Product_tessellated_item_step58)

require_command("AP242 tessellated product tree"
  "${MGED}" -c "${OUTPUT}" tree AP242_Tessellated_Product)
foreach(expected "${solid}/R" "${strip}/" "${shell}/" "${triangle_set}/"
    "${separate_solid}/R")
  require_text("${COMMAND_TEXT}" "${expected}" "AP242 tessellated hierarchy")
endforeach()

require_command("separate-coordinate tessellated solid inspection"
  "${MGED}" -c "${OUTPUT}" db get "${separate_solid}.s")
foreach(expected "bot mode volume orient rh" "4 faces")
  if(expected STREQUAL "4 faces")
    require_command("separate-coordinate tessellated solid summary"
      "${MGED}" -c "${OUTPUT}" l "${separate_solid}.s")
  endif()
  require_text("${COMMAND_TEXT}" "${expected}"
    "separate-coordinate closed AP242 tessellation")
endforeach()

require_command("closed tessellated solid inspection"
  "${MGED}" -c "${OUTPUT}" db get "${solid}.s")
foreach(expected
    "bot mode volume orient rh"
    "V { { 0 0 0 } { 0 10 0 } { 10 0 0 } { 0 0 10 }}"
    "F { { 0 1 2 } { 0 2 3 } { 0 3 1 } { 2 1 3 }}")
  require_text("${COMMAND_TEXT}" "${expected}" "closed AP242 tessellation")
endforeach()

foreach(surface IN ITEMS "${strip}" "${shell}" "${triangle_set}")
  require_command("surface tessellation ${surface} inspection"
    "${MGED}" -c "${OUTPUT}" db get "${surface}.s")
  require_text("${COMMAND_TEXT}" "bot mode surf orient rh"
    "surface AP242 tessellation ${surface}")
endforeach()

require_command("AP242 tessellated metadata inspection"
  "${MGED}" -c "${OUTPUT}" "attr show ${solid}.s")
foreach(expected
    "step:representation     AP242_TESSELLATED"
    "step:tessellated_intent solid"
    "step:tessellated_closed true"
    "step:style_name         solid tessellation style"
    "step:color_rgb          0.9 0.35 0.1"
    "step:layers             tessellated geometry")
  require_text("${COMMAND_TEXT}" "${expected}" "AP242 tessellated metadata")
endforeach()

# A TESSELLATED_SOLID whose edge incidence is not closed remains useful, but
# must be a surface BOT and must carry an explicit downgrade diagnostic.
set(open_output "${OUTPUT}.open.g")
set(open_report "${REPORT}.open.json")
file(REMOVE "${open_output}" "${open_report}")
execute_process(
  COMMAND "${STEP_G}" --strict -o "${open_output}" --report "${open_report}" "${OPEN_INPUT}"
  RESULT_VARIABLE open_result
  OUTPUT_VARIABLE open_stdout
  ERROR_VARIABLE open_stderr
)
if(NOT open_result EQUAL 0 OR NOT EXISTS "${open_output}")
  message(FATAL_ERROR
    "open AP242 tessellated solid was not preserved (${open_result}):\n"
    "${open_stdout}${open_stderr}")
endif()
file(READ "${open_report}" open_report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "preserved as a surface BOT")
  require_text("${open_report_text}" "${expected}" "open tessellated solid report")
endforeach()
set(open_solid AP242_Open_Tessellated_Solid_tessellated_item)
require_command("open tessellated solid BOT inspection"
  "${MGED}" -c "${open_output}" db get "${open_solid}.s")
require_text("${COMMAND_TEXT}" "bot mode surf orient rh" "open tessellated solid mode")
require_command("open tessellated solid metadata inspection"
  "${MGED}" -c "${open_output}" "attr show ${open_solid}.s")
foreach(expected
    "step:tessellated_intent solid"
    "step:tessellated_closed false")
  require_text("${COMMAND_TEXT}" "${expected}" "open tessellated solid metadata")
endforeach()

# Supplied normals are authoritative in exact mode.  Permissive safe repair
# may reverse triangle winding, while exact strict mode rejects the mismatch
# and must not publish a partial database.
file(READ "${INPUT}" reversed_text)
string(REPLACE "((0.,0.,1.))" "((0.,0.,-1.))" reversed_text "${reversed_text}")
set(reversed_input "${OUTPUT}.reversed.stp")
set(reversed_output "${OUTPUT}.reversed.g")
set(reversed_report "${REPORT}.reversed.json")
file(WRITE "${reversed_input}" "${reversed_text}")
file(REMOVE "${reversed_output}" "${reversed_report}")
execute_process(
  COMMAND "${STEP_G}" -o "${reversed_output}" --report "${reversed_report}" "${reversed_input}"
  RESULT_VARIABLE reversed_result
  OUTPUT_VARIABLE reversed_stdout
  ERROR_VARIABLE reversed_stderr
)
if(NOT reversed_result EQUAL 0 OR NOT EXISTS "${reversed_output}")
  message(FATAL_ERROR
    "safe winding repair failed (${reversed_result}):\n${reversed_stdout}${reversed_stderr}")
endif()
file(READ "${reversed_report}" reversed_report_text)
foreach(expected
    "\"repairs\":2"
    "reversed triangle winding to agree with supplied AP242 normals")
  require_text("${reversed_report_text}" "${expected}" "AP242 normal repair report")
endforeach()

set(exact_output "${OUTPUT}.reversed-exact.g")
set(exact_report "${REPORT}.reversed-exact.json")
file(REMOVE "${exact_output}" "${exact_report}")
execute_process(
  COMMAND "${STEP_G}" --strict --exact -o "${exact_output}"
    --report "${exact_report}" "${reversed_input}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_stdout
  ERROR_VARIABLE exact_stderr
)
if(exact_result EQUAL 0 OR EXISTS "${exact_output}")
  message(FATAL_ERROR
    "exact normal mismatch unexpectedly succeeded or published output:\n"
    "${exact_stdout}${exact_stderr}")
endif()
file(READ "${exact_report}" exact_report_text)
require_text("${exact_report_text}" "triangle winding opposes its supplied normal"
  "exact AP242 normal rejection")

# Invalid indices are malformed supported geometry, rather than an unknown
# representation type.  Both ordinary and strict imports must fail cleanly.
foreach(mode IN ITEMS ordinary strict)
  set(bad_output "${OUTPUT}.${mode}-malformed.g")
  set(bad_report "${REPORT}.${mode}-malformed.json")
  file(REMOVE "${bad_output}" "${bad_report}")
  set(mode_args)
  if(mode STREQUAL strict)
    list(APPEND mode_args --strict)
  endif()
  execute_process(
    COMMAND "${STEP_G}" ${mode_args} -o "${bad_output}" --report "${bad_report}"
      "${MALFORMED_INPUT}"
    RESULT_VARIABLE bad_result
    OUTPUT_VARIABLE bad_stdout
    ERROR_VARIABLE bad_stderr
  )
  if(bad_result EQUAL 0 OR EXISTS "${bad_output}")
    message(FATAL_ERROR
      "${mode} malformed tessellation unexpectedly succeeded or published output:\n"
      "${bad_stdout}${bad_stderr}")
  endif()
  file(READ "${bad_report}" bad_report_text)
  foreach(expected
      "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1,\"outcome\":\"failed\""
      "\"type\":\"TRIANGULATED_SURFACE_SET\",\"status\":\"malformed\""
      "triangle point-normal index is out of range")
    require_text("${bad_report_text}" "${expected}" "${mode} malformed tessellation report")
  endforeach()
endforeach()

# Generate a moderately large independent grid at test time.  This exercises
# aggregate parsing and BOT construction without storing a multi-thousand-line
# fixture in the source tree.
set(grid_cells 40)
math(EXPR grid_side "${grid_cells} + 1")
math(EXPR grid_points "${grid_side} * ${grid_side}")
math(EXPR grid_faces "2 * ${grid_cells} * ${grid_cells}")
set(coordinates)
set(triangles)
set(separator)
foreach(row RANGE 0 ${grid_cells})
  foreach(column RANGE 0 ${grid_cells})
    string(APPEND coordinates "${separator}(${column}.,${row}.,0.)")
    set(separator ",")
  endforeach()
endforeach()
set(separator)
math(EXPR grid_last "${grid_cells} - 1")
foreach(row RANGE 0 ${grid_last})
  foreach(column RANGE 0 ${grid_last})
    math(EXPR first "${row} * ${grid_side} + ${column} + 1")
    math(EXPR second "${first} + 1")
    math(EXPR third "${first} + ${grid_side}")
    math(EXPR fourth "${third} + 1")
    string(APPEND triangles
      "${separator}(${first},${second},${fourth}),(${first},${fourth},${third})")
    set(separator ",")
  endforeach()
endforeach()
set(large_input "${OUTPUT}.large.stp")
set(large_output "${OUTPUT}.large.g")
set(large_report "${REPORT}.large.json")
file(WRITE "${large_input}" "ISO-10303-21;\nHEADER;\n"
  "FILE_DESCRIPTION(('BRL-CAD AP242 tessellated grid performance fixture'),'2;1');\n"
  "FILE_NAME('ap242_tessellated_grid.stp','2026-08-01T00:00:00',('BRL-CAD'),('BRL-CAD'),'','','');\n"
  "FILE_SCHEMA(('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF'));\nENDSEC;\nDATA;\n"
  "#1=COORDINATES_LIST('grid coordinates',${grid_points},(${coordinates}));\n"
  "#2=TRIANGULATED_SURFACE_SET('grid triangles',#1,${grid_points},(),(),(${triangles}));\n"
  "#10=(LENGTH_UNIT() NAMED_UNIT(*) SI_UNIT(.MILLI.,.METRE.));\n"
  "#11=(NAMED_UNIT(*) PLANE_ANGLE_UNIT() SI_UNIT($,.RADIAN.));\n"
  "#12=(NAMED_UNIT(*) SI_UNIT($,.STERADIAN.) SOLID_ANGLE_UNIT());\n"
  "#13=(GEOMETRIC_REPRESENTATION_CONTEXT(3) GLOBAL_UNIT_ASSIGNED_CONTEXT((#10,#11,#12)) REPRESENTATION_CONTEXT('grid','3D'));\n"
  "#20=TESSELLATED_SHAPE_REPRESENTATION('',(#2),#13);\n"
  "#30=APPLICATION_CONTEXT('managed model based 3D engineering');\n"
  "#31=PRODUCT_CONTEXT('',#30,'mechanical');\n"
  "#32=PRODUCT_DEFINITION_CONTEXT('part definition',#30,'design');\n"
  "#33=PRODUCT('ap242_grid','AP242 Tessellated Grid','performance fixture',(#31));\n"
  "#34=PRODUCT_DEFINITION_FORMATION('1','initial revision',#33);\n"
  "#35=PRODUCT_DEFINITION('design','grid definition',#34,#32);\n"
  "#36=PRODUCT_DEFINITION_SHAPE('','',#35);\n"
  "#37=PROPERTY_DEFINITION_REPRESENTATION(#36,#20);\n"
  "ENDSEC;\nEND-ISO-10303-21;\n")
file(REMOVE "${large_output}" "${large_report}")
execute_process(
  COMMAND "${STEP_G}" --strict -o "${large_output}" --report "${large_report}" "${large_input}"
  RESULT_VARIABLE large_result
  OUTPUT_VARIABLE large_stdout
  ERROR_VARIABLE large_stderr
)
if(NOT large_result EQUAL 0 OR NOT EXISTS "${large_output}")
  message(FATAL_ERROR
    "large AP242 tessellation failed (${large_result}):\n${large_stdout}${large_stderr}")
endif()
file(READ "${large_report}" large_report_text)
require_text("${large_report_text}"
  "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\""
  "large AP242 tessellation report")
require_command("large AP242 tessellation BOT summary"
  "${MGED}" -c "${large_output}" l AP242_Tessellated_Grid_tessellated_item.s)
require_text("${COMMAND_TEXT}" "${grid_points} vertices, ${grid_faces} faces"
  "large AP242 tessellation BOT size")

# Some independent exporters retain zero-area strip triangles with a repeated
# point-normal index.  Safe mode may discard one only when the remaining solid
# is still demonstrably closed.  Exact mode must reject the source defect.
file(READ "${INPUT}" repeated_text)
string(REPLACE "((1,3,2),(1,2,4)" "((1,1,2),(1,3,2),(1,2,4)"
  repeated_text "${repeated_text}")
set(repeated_input "${OUTPUT}.repeated.stp")
set(repeated_output "${OUTPUT}.repeated.g")
set(repeated_report "${REPORT}.repeated.json")
file(WRITE "${repeated_input}" "${repeated_text}")
file(REMOVE "${repeated_output}" "${repeated_report}")
execute_process(
  COMMAND "${STEP_G}" -o "${repeated_output}" --report "${repeated_report}"
    "${repeated_input}"
  RESULT_VARIABLE repeated_result
  OUTPUT_VARIABLE repeated_stdout
  ERROR_VARIABLE repeated_stderr
)
if(NOT repeated_result EQUAL 0 OR NOT EXISTS "${repeated_output}")
  message(FATAL_ERROR
    "safe repeated-triangle repair failed (${repeated_result}):\n"
    "${repeated_stdout}${repeated_stderr}")
endif()
file(READ "${repeated_report}" repeated_report_text)
foreach(expected
    "\"geometry_attempted\":5,\"geometry_written\":5,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"repairs\":1"
    "discarded repeated-vertex triangles after proving the remaining tessellated solid is closed")
  require_text("${repeated_report_text}" "${expected}"
    "safe repeated-triangle report")
endforeach()
require_command("repaired repeated-triangle solid summary"
  "${MGED}" -c "${repeated_output}" l "${solid}.s")
require_text("${COMMAND_TEXT}" "4 vertices, 4 faces"
  "repaired repeated-triangle solid")

set(repeated_exact_output "${OUTPUT}.repeated-exact.g")
set(repeated_exact_report "${REPORT}.repeated-exact.json")
file(REMOVE "${repeated_exact_output}" "${repeated_exact_report}")
execute_process(
  COMMAND "${STEP_G}" --strict --exact -o "${repeated_exact_output}"
    --report "${repeated_exact_report}" "${repeated_input}"
  RESULT_VARIABLE repeated_exact_result
  OUTPUT_VARIABLE repeated_exact_stdout
  ERROR_VARIABLE repeated_exact_stderr
)
if(repeated_exact_result EQUAL 0 OR EXISTS "${repeated_exact_output}")
  message(FATAL_ERROR
    "exact repeated triangle unexpectedly succeeded or published output:\n"
    "${repeated_exact_stdout}${repeated_exact_stderr}")
endif()
file(READ "${repeated_exact_report}" repeated_exact_report_text)
require_text("${repeated_exact_report_text}" "triangle contains repeated vertices"
  "exact repeated-triangle rejection")
