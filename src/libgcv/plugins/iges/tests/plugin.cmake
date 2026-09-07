# BRL-CAD
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
# SPDX-License-Identifier: BSD-3-Clause

include("${CMAKE_CURRENT_LIST_DIR}/test_helpers.cmake")

file(REMOVE "${TEST_DIR}/source.g" "${TEST_DIR}/plugin.g" "${TEST_DIR}/cli.g")
file(WRITE "${TEST_DIR}/source.tcl"
  "opendb {${TEST_DIR}/source.g} y; units mm;\n"
  "in box.s rpp 0 10 0 20 0 30; in ball.s sph 10 20 30 4;\n"
  "r part.r u box.s - ball.s; comb all u part.r;\nq\n")
execute_process(COMMAND "${MGED}" -c INPUT_FILE "${TEST_DIR}/source.tcl"
  OUTPUT_FILE "${TEST_DIR}/setup.log" ERROR_FILE "${TEST_DIR}/setup.log"
  RESULT_VARIABLE status TIMEOUT 120)
if(NOT "${status}" STREQUAL "0")
  message(FATAL_ERROR "Plugin round-trip setup failed: ${status}")
endif()

run_checked(plugin-export "${GCV}" "${TEST_DIR}/source.g" "${TEST_DIR}/plugin.igs")
run_checked(cli-import "${IGESG}" --strict --repair none --report "${TEST_DIR}/cli.json"
  -o "${TEST_DIR}/cli.g" "${TEST_DIR}/plugin.igs")
check_complete_report("${TEST_DIR}/cli.json")
run_checked(plugin-export-rays "${RAY_CHECK}" "${TEST_DIR}/source.g" all
  "${TEST_DIR}/cli.g" iges_geometry)

run_checked(cli-export "${GIGES}" -o "${TEST_DIR}/cli.igs" "${TEST_DIR}/source.g" all)
run_checked(plugin-import "${GCV}" -I --strict --breps-only -i "${TEST_DIR}/cli.igs" -o "${TEST_DIR}/plugin.g")
run_checked(plugin-import-rays "${RAY_CHECK}" "${TEST_DIR}/source.g" all
  "${TEST_DIR}/plugin.g" unnamed)

# Exercise write-filter options through the executable's option forwarding.
run_checked(plugin-faceted "${GCV}" -O --faceted -i "${TEST_DIR}/source.g" -o "${TEST_DIR}/faceted.igs")
run_checked(faceted-import "${IGESG}" --strict -o "${TEST_DIR}/faceted.g" "${TEST_DIR}/faceted.igs")
