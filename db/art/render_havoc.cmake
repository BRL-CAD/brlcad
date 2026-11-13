# R E N D E R _ H A V O C . C M A K E
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
set(example_name havoc)
include("${CMAKE_CURRENT_LIST_DIR}/render_common.cmake")
find_program(oiiotool NAMES oiiotool HINTS "${BRLCAD_PREFIX}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)

file(REMOVE "${OUTPUT_DIR}/havoc-before.g" "${OUTPUT_DIR}/havoc-after.g"
  "${OUTPUT_DIR}/havoc-camera.sh" "${OUTPUT_DIR}/havoc-detail-camera.sh"
  "${OUTPUT_DIR}/havoc-studio.exr")
configure_file("${source_database}" "${OUTPUT_DIR}/havoc-before.g" COPYONLY)
run_tool(studio "art havoc studio ready" "${CMAKE_COMMAND}" -E env
  "ART_DEMO_OUTPUT_DIR=${OUTPUT_DIR}" -- "${mged}" -c "${OUTPUT_DIR}/havoc-before.g"
  "source havoc_studio.tcl")
configure_file("${OUTPUT_DIR}/havoc-before.g" "${OUTPUT_DIR}/havoc-after.g" COPYONLY)
run_tool(materials "art havoc materials ready" "${mged}" -c "${OUTPUT_DIR}/havoc-after.g"
  "source havoc_materials.tcl")

# A small scene-linear HDR panorama supplies broad warm, cool, and neutral
# studio panels.  Generating it here keeps the project independent of downloads.
run_tool(environment "" "${oiiotool}" --pattern constant:color=0.12,0.15,0.2 512x256 3
  --fill:color=5,4.5,4 60x40+200+40 --fill:color=2,3,4 35x50+320+55
  --fill:color=3,3,3 100x30+20+65 -d float -o "${OUTPUT_DIR}/havoc-studio.exr")
set(settings "samples=${SAMPLES} light_intensity=0 environment_map=havoc-studio.exr environment_intensity=2")
run_tool(materials-inspection "" "${art}" -c "set inspect_materials=1" -c "set ${settings}"
  "${OUTPUT_DIR}/havoc-after.g" art.demo.g)
read_saved_view("${OUTPUT_DIR}/havoc-camera.sh" full_camera)
read_saved_view("${OUTPUT_DIR}/havoc-detail-camera.sh" detail_camera)
foreach(variant IN ITEMS before after detail)
  if(variant STREQUAL "before")
    set(database "${OUTPUT_DIR}/havoc-before.g")
  else()
    set(database "${OUTPUT_DIR}/havoc-after.g")
  endif()
  if(variant STREQUAL "detail")
    set(camera "${detail_camera}")
  else()
    set(camera "${full_camera}")
  endif()
  set(view "${OUTPUT_DIR}/havoc-${variant}.view")
  set(image "${OUTPUT_DIR}/havoc-${variant}.png")
  write_example_file("${view}" "set ${settings};\n${camera}\n")
  file(REMOVE "${image}")
  run_tool(${variant} "" INPUT_FILE "${view}" "${art}" -P ${WORKERS} -w ${WIDTH} -n ${HEIGHT}
    -M -o "${image}" "${database}" art.demo.g)
endforeach()
message(STATUS "Rendered Havoc material comparison in ${OUTPUT_DIR}")
