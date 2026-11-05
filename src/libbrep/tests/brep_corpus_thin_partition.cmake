# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1.

if(NOT DEFINED MGED OR NOT DEFINED BREP_CORPUS OR NOT DEFINED TEST_DB OR
   NOT DEFINED REPORT)
  message(FATAL_ERROR "MGED, BREP_CORPUS, TEST_DB, and REPORT must be defined")
endif()

get_filename_component(TEST_DIR "${TEST_DB}" DIRECTORY)
file(MAKE_DIRECTORY "${TEST_DIR}")
file(REMOVE "${TEST_DB}" "${REPORT}")

function(run_mged COMMAND_TEXT)
  execute_process(
    COMMAND "${MGED}" -c "${TEST_DB}" "${COMMAND_TEXT}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE STDOUT
    ERROR_VARIABLE STDERR)
  if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR
      "${COMMAND_TEXT} failed with ${RESULT}: ${STDOUT}${STDERR}")
  endif()
endfunction()

# The first component supplies ordinary volume and matching partitions.  The
# second is a broad 0.00045 mm slab represented as a box subtraction.  CSG and
# B-Rep shotline cleanup disagree about that real but sub-tolerance material on
# more than one percent of rays, while chord and volume errors stay small.
run_mged("in main.s rpp 0 10 0 10 0 10")
run_mged("in slab-outer.s rpp 0 10 12 22 0 10")
run_mged("in slab-inner.s rpp 0 10 12.00045 22 0 10")
run_mged("put thin-partition.r comb region yes tree {u {l main.s} {- {l slab-outer.s} {l slab-inner.s}}}")

execute_process(
  COMMAND "${BREP_CORPUS}"
    --database "${TEST_DB}"
    --component thin-partition.r
    --output "${REPORT}"
    --rays 4096
    --timeout-ms 30000
    --require-semantic-pass
  RESULT_VARIABLE CORPUS_RESULT
  OUTPUT_VARIABLE CORPUS_STDOUT
  ERROR_VARIABLE CORPUS_STDERR)
if(NOT CORPUS_RESULT EQUAL 0)
  message(FATAL_ERROR
    "thin-partition corpus run failed with ${CORPUS_RESULT}: "
    "${CORPUS_STDOUT}${CORPUS_STDERR}")
endif()

file(READ "${REPORT}" REPORT_TEXT)
# Exact agreement is a stronger outcome and can become possible as Boolean
# topology improves.  Otherwise the valid solid must be retained with the
# explicit thin-partition note.
if(NOT REPORT_TEXT MATCHES
   ",(success|approximate_success),0,[^\n]*,1,1,pass," AND
   NOT REPORT_TEXT MATCHES
   ",thin_partition_divergence,0,[^\n]*,1,1,retained_with_note,")
  message(FATAL_ERROR
    "thin-partition result was neither a semantic pass nor retained with a note:\n${REPORT_TEXT}")
endif()
