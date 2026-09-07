# BRL-CAD
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
# SPDX-License-Identifier: BSD-3-Clause

include("${CMAKE_CURRENT_LIST_DIR}/test_helpers.cmake")

run_checked(asc2g "${ASC2G}" "${MODEL}" "${TEST_DIR}/source.g")
if(CASE STREQUAL "instances")
  # Keep the model's scaled, nested cube assembly.  Its half-space ground
  # and environment sphere are not part of this bounded CSG comparison.
  run_checked(select "${MGED}" -c "${TEST_DIR}/source.g" "rm all.g/plate.r all.g/envmap.r")
elseif(CASE STREQUAL "pipe")
  # Straight pipe permits a native ray comparison; the bent model also
  # exercises the exchange independently of the BRep ray tracer below.
  run_checked(select-remove "${MGED}" -c "${TEST_DIR}/source.g" "kill pipe.s")
  run_checked(select "${MGED}" -c "${TEST_DIR}/source.g"
    "put pipe.s pipe V0 {250 250 0} O0 250 I0 62.5 R0 250 V1 {250 250 1750} O1 250 I1 62.5 R1 250")
elseif(CASE STREQUAL "extrusion")
  # A simple closed profile provides an unambiguous native ray reference.
  # Keep the original sketch (including its hole) in extrusion_hole.
  run_checked(select "${MGED}" -c "${TEST_DIR}/source.g"
    "db adjust sketch VL {{0 0} {500 0} {500 1000} {0 1000}} SL {{line S 0 E 1} {line S 1 E 2} {line S 2 E 3} {line S 3 E 0}}")
elseif(CASE STREQUAL "bspline")
  # A curved quadratic patch, not a solid.  Point type 100 is three
  # non-rational XYZ coordinates in the legacy NURBS representation.
  run_checked(select "${MGED}" -c "${TEST_DIR}/source.g"
    "put legacy.s bspline N 1 S {{O {3 3} s {3 3} T 100 u {0 0 0 1 1 1} v {0 0 0 1 1 1} P {0 0 0 0 5 5 0 10 0 5 0 5 5 5 10 5 10 5 10 0 0 10 5 5 10 10 0}}}")
endif()
run_checked(export "${GIGES}" -o "${TEST_DIR}/export.igs" "${TEST_DIR}/source.g" "${OBJECT}")
if(ENTITY_TYPE STREQUAL "128")
  file(READ "${TEST_DIR}/export.log" export_log)
  if(export_log MATCHES "solids converted to NMG")
    message(FATAL_ERROR "Primitive B-Rep export unexpectedly used tessellation")
  endif()
endif()
file(STRINGS "${TEST_DIR}/export.igs" records)
set(found FALSE)
foreach(record IN LISTS records)
  string(LENGTH "${record}" length)
  if(length LESS 80)
    continue()
  endif()
  string(SUBSTRING "${record}" 72 1 section)
  string(SUBSTRING "${record}" 0 8 type)
  string(STRIP "${type}" type)
  if(section STREQUAL "D" AND type MATCHES "^(${ENTITY_TYPE})$")
    set(found TRUE)
  endif()
endforeach()
if(NOT found)
  message(FATAL_ERROR "Round trip did not exercise IGES entity type ${ENTITY_TYPE}")
endif()

foreach(mode default strict)
  set(flags)
  if(mode STREQUAL "strict")
    set(flags --strict --repair none)
  endif()
  run_checked(${mode} "${IGESG}" ${flags} --report "${TEST_DIR}/${mode}.json"
    -o "${TEST_DIR}/${mode}.g" "${TEST_DIR}/export.igs")
  check_complete_report("${TEST_DIR}/${mode}.json")
  if(ENTITY_TYPE STREQUAL "128")
    string(REGEX REPLACE "\\.r$" ".s" primitive "${OBJECT}")
    string(REPLACE "." "_" imported "${primitive}")
    if(CASE STREQUAL "bspline")
      execute_process(COMMAND "${MGED}" -c "${TEST_DIR}/${mode}.g" "search -type brep"
        RESULT_VARIABLE search_status OUTPUT_VARIABLE imported ERROR_VARIABLE search_error)
      string(APPEND imported "${search_error}")
      string(STRIP "${imported}" imported)
      if(NOT search_status EQUAL 0 OR imported STREQUAL "" OR imported MATCHES "[\r\n]")
        message(FATAL_ERROR "Expected one imported BRep: ${imported} ${search_error}")
      endif()
    endif()
    set(orientation_flags)
    if(CASE MATCHES "^extrusion(_hole)?$")
      # These callbacks can produce consistently inward B-Reps.  Require
      # outward output, allowing only a whole-shell reversal of the reference.
      set(orientation_flags allow-global-reversal)
      file(READ "${TEST_DIR}/${mode}.json" report)
      string(JSON outward GET "${report}" orientation statistics outward)
      string(JSON inward GET "${report}" orientation statistics inward)
      string(JSON uncertain GET "${report}" orientation statistics indeterminate)
      if(NOT outward EQUAL 1 OR NOT inward EQUAL 0 OR NOT uncertain EQUAL 0)
        message(FATAL_ERROR "Extrusion exchange did not produce a resolved outward shell: ${report}")
      endif()
    endif()
    run_checked(${mode}-brep "${BREP_CHECK}" "${TEST_DIR}/source.g" "${primitive}"
      "${TEST_DIR}/${mode}.g" "${imported}" ${orientation_flags})
  endif()
  # CLINE has ray-dependent plate thickness; it cannot have identical LOS
  # behavior as a fixed wall solid.  For these complex cases compare the
  # callback's actual geometry and topology, not the native ray routines.
  if(NOT CASE MATCHES "^(cline|pipe_bend|extrusion_hole|bspline)$")
    run_checked(${mode}-rays "${RAY_CHECK}" "${TEST_DIR}/source.g" "${OBJECT}"
      "${TEST_DIR}/${mode}.g" iges_geometry)
  endif()
endforeach()
