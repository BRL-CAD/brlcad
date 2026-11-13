# R E N D E R _ C O M M O N . C M A K E
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

if(NOT DEFINED BRLCAD_PREFIX OR NOT IS_DIRECTORY "${BRLCAD_PREFIX}")
  message(FATAL_ERROR "BRLCAD_PREFIX must name an installation containing art and mged")
endif()
find_program(art NAMES art HINTS "${BRLCAD_PREFIX}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
find_program(mged NAMES mged HINTS "${BRLCAD_PREFIX}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
if(NOT DEFINED SOURCE_DB)
  set(SOURCE_DB "${BRLCAD_PREFIX}/share/db/${example_name}.g")
endif()
if(NOT EXISTS "${SOURCE_DB}" OR IS_DIRECTORY "${SOURCE_DB}")
  message(FATAL_ERROR "SOURCE_DB must name the installed ${example_name}.g example database")
endif()
file(REAL_PATH "${SOURCE_DB}" source_database)
if(NOT DEFINED OUTPUT_DIR)
  set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${example_name}-art")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(REAL_PATH "${OUTPUT_DIR}" OUTPUT_DIR)
# The driver owns generated files and logs in this directory.  Keep its input
# outside that ownership boundary, including aliases resolved by REAL_PATH.
get_filename_component(source_name "${SOURCE_DB}" ABSOLUTE)
foreach(input_path IN ITEMS "${source_name}" "${source_database}")
  file(RELATIVE_PATH source_relative "${OUTPUT_DIR}" "${input_path}")
  if(NOT IS_ABSOLUTE "${source_relative}" AND NOT source_relative MATCHES "^\\.\\.(/|$)")
    message(FATAL_ERROR "SOURCE_DB must be outside the generated OUTPUT_DIR")
  endif()
endforeach()
set(default_WIDTH 512)
set(default_HEIGHT 384)
set(default_SAMPLES 64)
set(default_WORKERS 2)
foreach(setting IN ITEMS WIDTH HEIGHT SAMPLES WORKERS)
  if(NOT DEFINED ${setting})
    set(${setting} "${default_${setting}}")
  endif()
  if(NOT "${${setting}}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "${setting} must be a positive integer")
  endif()
endforeach()
set(tool_environment "${CMAKE_COMMAND}" -E env)
foreach(variable IN ITEMS BRLCAD_ROOT LD_LIBRARY_PATH LD_PRELOAD DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH)
  list(APPEND tool_environment "--unset=${variable}")
endforeach()
list(APPEND tool_environment --)
# Replace owned files without following an existing symbolic or hard link.
function(write_example_file path content)
  file(REMOVE "${path}")
  file(WRITE "${path}" "${content}")
endfunction()

function(run_tool label sentinel)
  cmake_parse_arguments(PARSE_ARGV 2 tool "" "INPUT_FILE" "")
  set(input)
  if(DEFINED tool_INPUT_FILE)
    set(input INPUT_FILE "${tool_INPUT_FILE}")
  endif()
  execute_process(COMMAND ${tool_environment} ${tool_UNPARSED_ARGUMENTS} ${input}
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE output)
  write_example_file("${OUTPUT_DIR}/${label}.log" "${output}")
  if(NOT "${status}" STREQUAL "0")
    message(FATAL_ERROR "${label} failed (${status}):\n${output}")
  endif()
  # mged can report script errors without returning a failing process status.
  if(NOT "${sentinel}" STREQUAL "")
    string(FIND "${output}" "${sentinel}" completed)
    if(completed LESS 0)
      message(FATAL_ERROR "${label} did not complete:\n${output}")
    endif()
  endif()
endfunction()


function(read_saved_view path output)
  file(READ "${path}" saved)
  string(REPLACE "\r\n" "\n" saved "${saved}")
  set(marker "<<EOF\n")
  string(FIND "${saved}" "${marker}" begin)
  if(begin LESS 0)
    message(FATAL_ERROR "Saved view ${path} has no command-stream delimiter")
  endif()
  string(LENGTH "${marker}" marker_length)
  math(EXPR begin "${begin} + ${marker_length}")
  string(SUBSTRING "${saved}" ${begin} -1 stream)
  string(FIND "${stream}" "\nEOF" end)
  if(end LESS 0)
    message(FATAL_ERROR "Saved view ${path} has no command-stream terminator")
  endif()
  string(SUBSTRING "${stream}" 0 ${end} stream)
  set(${output} "${stream}" PARENT_SCOPE)
endfunction()


function(render_studio_comparison environment_intensity)
  find_program(oiiotool NAMES oiiotool HINTS "${BRLCAD_PREFIX}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)

  file(REMOVE "${OUTPUT_DIR}/${example_name}-before.g" "${OUTPUT_DIR}/${example_name}-after.g"
    "${OUTPUT_DIR}/${example_name}-camera.sh" "${OUTPUT_DIR}/${example_name}-detail-camera.sh"
    "${OUTPUT_DIR}/${example_name}-studio.exr")
  configure_file("${source_database}" "${OUTPUT_DIR}/${example_name}-before.g" COPYONLY)
  run_tool(studio "art ${example_name} studio ready" "${CMAKE_COMMAND}" -E env
    "ART_DEMO_OUTPUT_DIR=${OUTPUT_DIR}" -- "${mged}" -c "${OUTPUT_DIR}/${example_name}-before.g"
    "source ${example_name}_studio.tcl")
  configure_file("${OUTPUT_DIR}/${example_name}-before.g" "${OUTPUT_DIR}/${example_name}-after.g" COPYONLY)
  run_tool(materials "art ${example_name} materials ready" "${mged}" -c "${OUTPUT_DIR}/${example_name}-after.g"
    "source ${example_name}_materials.tcl")

  # A small scene-linear HDR panorama supplies broad warm, cool, and neutral
  # studio panels.  Generating it here keeps the project independent of downloads.
  run_tool(environment "" "${oiiotool}" --pattern constant:color=0.12,0.15,0.2 512x256 3
    --fill:color=5,4.5,4 60x40+200+40 --fill:color=2,3,4 35x50+320+55
    --fill:color=3,3,3 100x30+20+65 -d float -o "${OUTPUT_DIR}/${example_name}-studio.exr")
  set(settings "samples=${SAMPLES} light_intensity=0 environment_map=${example_name}-studio.exr environment_intensity=${environment_intensity}")
  run_tool(materials-inspection "" "${art}" -c "set inspect_materials=1" -c "set ${settings}"
    "${OUTPUT_DIR}/${example_name}-after.g" art.demo.g)
  read_saved_view("${OUTPUT_DIR}/${example_name}-camera.sh" full_camera)
  read_saved_view("${OUTPUT_DIR}/${example_name}-detail-camera.sh" detail_camera)
  foreach(variant IN ITEMS before after detail)
    if(variant STREQUAL "before")
      set(database "${OUTPUT_DIR}/${example_name}-before.g")
    else()
      set(database "${OUTPUT_DIR}/${example_name}-after.g")
    endif()
    if(variant STREQUAL "detail")
      set(camera "${detail_camera}")
    else()
      set(camera "${full_camera}")
    endif()
    set(view "${OUTPUT_DIR}/${example_name}-${variant}.view")
    set(image "${OUTPUT_DIR}/${example_name}-${variant}.png")
    write_example_file("${view}" "set ${settings};\n${camera}\n")
    file(REMOVE "${image}")
    message(STATUS "Rendering ${example_name}-${variant}: ${WIDTH}x${HEIGHT}, ${SAMPLES} samples")
    run_tool(${variant} "" INPUT_FILE "${view}" "${art}" -P ${WORKERS} -w ${WIDTH} -n ${HEIGHT}
      -M -o "${image}" "${database}" art.demo.g)
  endforeach()
  message(STATUS "Rendered ${example_name} material comparison in ${OUTPUT_DIR}")
endfunction()
