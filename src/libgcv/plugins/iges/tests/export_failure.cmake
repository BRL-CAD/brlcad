# BRL-CAD
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
# SPDX-License-Identifier: BSD-3-Clause

include("${CMAKE_CURRENT_LIST_DIR}/test_helpers.cmake")

if(CASE STREQUAL "metadata")
  # Unsigned byte metadata has the same minor type ID as an ARB8.
  file(WRITE "${TEST_DIR}/metadata.dat" "Non-geometric metadata\n")
  set(failed_object metadata.bin)
  set(create_failure "bo -i u C ${failed_object} metadata.dat")
  set(diagnostic "is non-geometric data")
elseif(CASE STREQUAL "inward_shell")
  # A closed tetrahedron with inward winding has no outer shell.
  set(failed_object inward.s)
  set(create_failure
    "put ${failed_object} bot mode volume orient rh flags {} V {{0 0 0} {10 0 0} {0 10 0} {0 0 10}} F {{0 1 2} {0 3 1} {0 2 3} {1 3 2}}")
  set(diagnostic "has no outer shell")
elseif(CASE STREQUAL "submodel")
  # A generic fallback reaches a SUBMODEL tessellator with a null region.
  # Keep this unsupported type outside the qualified IGES callback set.
  set(failed_object submodel.s)
  set(create_failure "put ${failed_object} submodel file {} treetop valid.c meth 0")
  set(diagnostic "primitive type is not supported")
else()
  message(FATAL_ERROR "Unknown export failure case: ${CASE}")
endif()

file(WRITE "${TEST_DIR}/fixture.tcl"
  "in left.s rpp 0 10 0 10 0 10\n"
  "in right.s rpp 20 30 0 10 0 10\n"
  "put valid.c comb tree {u {l left.s} {l right.s}}\n"
  "facetize -n valid.nmg valid.c\n"
  "${create_failure}\n"
  "put mixed comb tree {u {l valid.nmg} {l ${failed_object}}}\n"
  "q\n")
file(REMOVE "${TEST_DIR}/source.g")
run_checked(create "${MGED}" -c "${TEST_DIR}/source.g" "source fixture.tcl")

foreach(selection failed_only mixed_roots mixed_combination)
  if(selection STREQUAL "failed_only")
    set(objects "${failed_object}")
  elseif(selection STREQUAL "mixed_roots")
    set(objects "${failed_object}" valid.nmg)
  else()
    set(objects mixed)
  endif()
  execute_process(COMMAND "${GIGES}" -o "${TEST_DIR}/${selection}.igs"
    "${TEST_DIR}/source.g" ${objects} RESULT_VARIABLE status
    OUTPUT_FILE "${TEST_DIR}/${selection}.log" ERROR_FILE "${TEST_DIR}/${selection}.log"
    WORKING_DIRECTORY "${TEST_DIR}" TIMEOUT 120)
  file(READ "${TEST_DIR}/${selection}.log" output)
  if(NOT "${status}" STREQUAL "1" OR NOT output MATCHES "${diagnostic}")
    message(FATAL_ERROR "Expected a diagnosed export failure, not an abort (${status}):\n${output}")
  endif()
  if(NOT selection STREQUAL "failed_only")
    run_checked(${selection}-import "${IGESG}" --strict --repair none
      --report "${TEST_DIR}/${selection}.json" -o "${TEST_DIR}/${selection}.g"
      "${TEST_DIR}/${selection}.igs")
    check_complete_report("${TEST_DIR}/${selection}.json")
    run_checked(${selection}-rays "${RAY_CHECK}" "${TEST_DIR}/source.g" valid.c
      "${TEST_DIR}/${selection}.g" iges_geometry)
  endif()
endforeach()
