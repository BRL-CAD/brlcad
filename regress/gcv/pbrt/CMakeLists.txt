# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
# SPDX-License-Identifier: BSD-2-Clause

if(TARGET gcv-assetimport)
  brlcad_regression_test(regress-gcv-pbrt "gcv;gcv-assetimport"
    TEST_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-gcv-pbrt.cmake.in" EXEC gcv)
  set_tests_properties(regress-gcv-pbrt PROPERTIES
    SKIP_REGULAR_EXPRESSION "PBRT exporter unavailable")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/scene.pbrt" "${CMAKE_CURRENT_BINARY_DIR}/relative.pbrt")
endif()

cmakefiles(CMakeLists.txt regress-gcv-pbrt.cmake.in)
