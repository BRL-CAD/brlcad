# BRL-CAD
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
# SPDX-License-Identifier: BSD-3-Clause

include("${CMAKE_CURRENT_LIST_DIR}/test_helpers.cmake")

# Fixtures use centimeters and nonzero origins; the independent references
# are specified in millimeters.  These cases must not rely on g-iges emitting
# the same native type (ARB wedges, for example, export as BReps).
file(REMOVE "${TEST_DIR}/reference.g")
set(setup "opendb {${TEST_DIR}/reference.g} y; units mm;")
if(CASE STREQUAL "wedge")
  string(APPEND setup
    " in trapezoid arb8 10 20 30 110 20 30 30 80 30 10 80 30 10 20 70 110 20 70 30 80 70 10 80 70;"
    " in triangle arb8 200 20 30 200 120 30 140 20 30 140 20 30 200 20 70 200 120 70 140 20 70 140 20 70;")
  set(pairs "trapezoid,wedge.0" "triangle,wedge.1")
elseif(CASE STREQUAL "revolve")
  string(APPEND setup
    " in cone trc 10 20 30 0 0 50 20 40;"
    " in positive_y rpp -30 50 20 60 30 80;"
    " in positive_xy rpp 10 50 20 60 30 80;"
    " in negative_y_positive_x rpp 10 50 -20 20 30 80;"
    " comb quarter u cone + positive_xy;"
    " comb half u cone + positive_y;"
    " comb three_quarters u cone - negative_y_positive_x;")
  set(pairs "cone,revolution.1" "quarter,revolution.2"
    "half,revolution.3" "three_quarters,revolution.4")
elseif(CASE STREQUAL "extrude")
  string(APPEND setup
    " in cylinder rcc 10 20 30 0 0 50 20;"
    " in oblique tgc 10 20 30 0 30 40 20 0 0 0 20 0 20 20;"
    " in transformed rcc 80 10 30 0 0 50 20;")
  set(pairs "cylinder,extrusion.1" "oblique,extrusion.2" "transformed,extrusion.5")
else()
  message(FATAL_ERROR "Unknown native case ${CASE}")
endif()
file(WRITE "${TEST_DIR}/reference.tcl" "${setup}\nq\n")
execute_process(COMMAND "${MGED}" -c INPUT_FILE "${TEST_DIR}/reference.tcl"
  OUTPUT_FILE "${TEST_DIR}/reference.log" ERROR_FILE "${TEST_DIR}/reference.log"
  RESULT_VARIABLE status TIMEOUT 120)
if(NOT "${status}" STREQUAL "0")
  message(FATAL_ERROR "Reference geometry setup failed: ${status}")
endif()

foreach(mode default strict)
  set(flags)
  if(mode STREQUAL "strict")
    set(flags --strict --repair none)
  endif()
  run_checked(${mode} "${IGESG}" ${flags} --report "${TEST_DIR}/${mode}.json"
    -o "${TEST_DIR}/${mode}.g" "${FIXTURE}")
  check_complete_report("${TEST_DIR}/${mode}.json")
  foreach(pair IN LISTS pairs)
    string(REPLACE "," ";" fields "${pair}")
    list(GET fields 0 reference)
    list(GET fields 1 object)
    run_checked(${mode}-${reference} "${RAY_CHECK}"
      "${TEST_DIR}/reference.g" "${reference}" "${TEST_DIR}/${mode}.g" "${object}")
  endforeach()
endforeach()

run_checked(export "${GIGES}" -o "${TEST_DIR}/export.igs" "${TEST_DIR}/default.g" iges_geometry)
run_checked(reimport "${IGESG}" --strict --repair none -o "${TEST_DIR}/roundtrip.g" "${TEST_DIR}/export.igs")
run_checked(roundtrip-rays "${RAY_CHECK}" "${TEST_DIR}/default.g" iges_geometry
  "${TEST_DIR}/roundtrip.g" iges_geometry)
