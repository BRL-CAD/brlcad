# R E N D E R _ S T A R . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#

cmake_minimum_required(VERSION 3.22)
set(example_name star)
include("${CMAKE_CURRENT_LIST_DIR}/render_common.cmake")
find_program(rt NAMES rt HINTS "${BRLCAD_PREFIX}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)

foreach(variant IN ITEMS before after)
  file(REMOVE "${OUTPUT_DIR}/star-${variant}.g")
  configure_file("${source_database}" "${OUTPUT_DIR}/star-${variant}.g" COPYONLY)
endforeach()
# saveview appends to an existing script; each run needs a fresh camera.
file(REMOVE "${OUTPUT_DIR}/star-camera.sh")
run_tool(studio "art sky demo ready" "${CMAKE_COMMAND}" -E env
  "ART_DEMO_OUTPUT_DIR=${OUTPUT_DIR}" -- "${mged}" -c "${OUTPUT_DIR}/star-after.g"
  "source star_sky.tcl")

# Use mged's subject-framed camera for both renderers.  Including the huge
# legacy shell in automatic framing would make the ship disappear.
read_saved_view("${OUTPUT_DIR}/star-camera.sh" stream)
set(camera "${OUTPUT_DIR}/star-camera.view")
file(WRITE "${camera}" "${stream}\n")
# Native rt can resume an existing output as raw scanlines.  Start clean
# rather than letting a previous PNG be interpreted as a partial PIX file.
file(REMOVE "${OUTPUT_DIR}/star-before.png" "${OUTPUT_DIR}/star-after.png"
  "${OUTPUT_DIR}/star-space.png")
set(render_args -P ${WORKERS} -w ${WIDTH} -n ${HEIGHT} -M)
run_tool(before "" INPUT_FILE "${camera}" "${rt}" ${render_args} -A 0.4
  -o "${OUTPUT_DIR}/star-before.png" "${OUTPUT_DIR}/star-before.g" all)

# Point-light intensity is expressed in this model's millimeter coordinates.
# The white dome also supplies neutral secondary-ray illumination.
set(lighting "environment_color=white environment_intensity=0.08 light_intensity=1e11")
set(background "background_color=0/0/150 background_intensity=1")
run_tool(materials-inspection "" "${art}" -c "set inspect_materials=1"
  -c "set ${lighting} ${background}" "${OUTPUT_DIR}/star-after.g" art.demo.g)
foreach(variant IN ITEMS after space)
  if(variant STREQUAL "space")
    set(background "background_color=black background_intensity=1")
  endif()
  set(view "${OUTPUT_DIR}/star-${variant}.view")
  file(WRITE "${view}" "set samples=${SAMPLES} ${lighting} ${background};\n${stream}\n")
  run_tool(${variant} "" INPUT_FILE "${view}" "${art}" ${render_args}
    -o "${OUTPUT_DIR}/star-${variant}.png" "${OUTPUT_DIR}/star-after.g" art.demo.g)
endforeach()
message(STATUS "Rendered star sky comparison in ${OUTPUT_DIR}")
