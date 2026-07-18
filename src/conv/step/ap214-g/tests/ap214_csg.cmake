if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "ap214-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"styles_extracted\":1"
    "\"styles_applied\":1"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"CSG_SOLID\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree CSG_Product
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}\n${tree_error}")
if(NOT tree_text MATCHES "CSG_Product_csg_item" OR
   NOT tree_text MATCHES "CSG_Product_csg_node_step14")
  message(FATAL_ERROR "native CSG hierarchy validation failed\n${tree_output}\n${tree_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_primitive.s
  OUTPUT_VARIABLE block_output
  ERROR_VARIABLE block_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_primitive_step10.s
  OUTPUT_VARIABLE cylinder_output
  ERROR_VARIABLE cylinder_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_primitive_step13.s
  OUTPUT_VARIABLE sphere_output
  ERROR_VARIABLE sphere_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_primitive_step62.s
  OUTPUT_VARIABLE torus_output
  ERROR_VARIABLE torus_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_primitive_step66.s
  OUTPUT_VARIABLE wedge_output
  ERROR_VARIABLE wedge_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_primitive_step70.s
  OUTPUT_VARIABLE cone_output
  ERROR_VARIABLE cone_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_half.s
  OUTPUT_VARIABLE half_output
  ERROR_VARIABLE half_error
)
set(block_text "${block_output}\n${block_error}")
set(cylinder_text "${cylinder_output}\n${cylinder_error}")
set(sphere_text "${sphere_output}\n${sphere_error}")
set(torus_text "${torus_output}\n${torus_error}")
set(wedge_text "${wedge_output}\n${wedge_error}")
set(cone_text "${cone_output}\n${cone_error}")
set(half_text "${half_output}\n${half_error}")
string(FIND "${block_text}" "arb8 V1" block_found)
string(FIND "${cylinder_text}" "tgc V" cylinder_found)
string(FIND "${sphere_text}" "ell V" sphere_found)
string(FIND "${torus_text}" "tor V" torus_found)
string(FIND "${wedge_text}" "arb8 V1" wedge_found)
string(FIND "${cone_text}" "tgc V" cone_found)
string(FIND "${half_text}" "half N" half_found)
if(block_found EQUAL -1 OR cylinder_found EQUAL -1 OR sphere_found EQUAL -1 OR
   torus_found EQUAL -1 OR wedge_found EQUAL -1 OR cone_found EQUAL -1 OR half_found EQUAL -1)
  message(FATAL_ERROR "native primitive validation failed\nblock: ${block_output} ${block_error}\ncylinder: ${cylinder_output} ${cylinder_error}\nsphere: ${sphere_output} ${sphere_error}\ntorus: ${torus_output} ${torus_error}\nwedge: ${wedge_output} ${wedge_error}\ncone: ${cone_output} ${cone_error}\nhalf: ${half_output} ${half_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show CSG_Product_csg_item
  OUTPUT_VARIABLE csg_attr_output
  ERROR_VARIABLE csg_attr_error
)
set(csg_attr_text "${csg_attr_output}\n${csg_attr_error}")
if(NOT csg_attr_text MATCHES "step:source_id[ ]+15" OR
   NOT csg_attr_text MATCHES "step:color_rgb[ ]+0.8 0.3 0.1")
  message(FATAL_ERROR "native CSG region metadata validation failed\n${csg_attr_output}\n${csg_attr_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get CSG_Product_csg_item
  OUTPUT_VARIABLE csg_region_output
  ERROR_VARIABLE csg_region_error
)
set(csg_region_text "${csg_region_output}\n${csg_region_error}")
if(NOT csg_region_text MATCHES "comb region yes" OR
   NOT csg_region_text MATCHES "rgb \{204 77 26\}")
  message(FATAL_ERROR "native CSG region semantics validation failed\n${csg_region_output}\n${csg_region_error}")
endif()
