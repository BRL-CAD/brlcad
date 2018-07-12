# There is quite a bit of shared logic for the various converters when it comes
# to building .g outputs, so we use a single function to manage the process and
# handle format-specific bits internally.  TODO - we probably need to add
# optional argument support here later to support (say) specifying a specific
# COMGEOM or STEP format version...

function(ADD_G_TARGET in_model folder)

  # Initialize
  set(MODEL_TYPE "UNKNOWN")

  # Get our root path
  if(CMAKE_CONFIGURATION_TYPES)
    set(bin_root "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
  else(CMAKE_CONFIGURATION_TYPES)
    set(bin_root "${CMAKE_BINARY_DIR}")
  endif(CMAKE_CONFIGURATION_TYPES)

  # Find our conversion type
  set(model_types "3dm;asc;comgeom;fg4;stp")
  foreach(mt ${model_types})
    if("${MODEL_TYPE}" STREQUAL "UNKNOWN")
      if("${in_model}" MATCHES ".${mt}$")
	set(MODEL_TYPE "${mt}")
	string(REGEX REPLACE "([0-9a-z]*).${mt}" "\\1" in_model_root "${in_model}")
      endif("${in_model}" MATCHES ".${mt}$")
    endif("${MODEL_TYPE}" STREQUAL "UNKNOWN")
  endforeach(mt ${model_types})
  if("${MODEL_TYPE}" STREQUAL "UNKNOWN")
    message(FATAL_ERROR "Unable to identify input model type for file ${in_model}")
  endif("${MODEL_TYPE}" STREQUAL "UNKNOWN")

  # Find our output file and path
  string(REPLACE "${CMAKE_SOURCE_DIR}/" "" rel_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  set(output_file ${DATA_DIR}/${rel_dir}/${in_model_root}.g)
  set(log_file ${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.log)

  # Make sure the output db target directories are there
  if(CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${CFG_TYPE}/${DATA_DIR}/${rel_dir}")
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  else(CMAKE_CONFIGURATION_TYPES)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${DATA_DIR}/${rel_dir}")
  endif(CMAKE_CONFIGURATION_TYPES)

  #############################################################################
  #                            Build logic
  #############################################################################

  # The 3DM conversion is rather different than the general pattern at the
  # moment (However, the 3dm approach is probably the "wave of the future" in
  # the sense that it uses libgcv...)
  if(${MODEL_TYPE} STREQUAL "3dm")

    # Because we won't know the output directory in the script reliably until run time,
    # we append CURRENT_BUILD_DIR as a string to the arguments - it is the script's
    # responsibility to handle it correctly at run time.
    set(CMD_ARGS "-o" "CURRENT_BUILD_DIR/${output_file}" "${CMAKE_CURRENT_SOURCE_DIR}/${in_model}")

    # Use the model name for the script, since the output file is specific to this input
    set(script_file "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake")

    # We know enough - write the script
    generate_cmd_script(3dm-g "${script_file}" OLOG "${log_file}" ELOG "${log_file}" CARGS "${CMD_ARGS}")

    # Define the target
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
      COMMAND ${CMAKE_COMMAND} -P "${script_file}"
      DEPENDS 3dm-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )

  endif(${MODEL_TYPE} STREQUAL "3dm")

  # The rest of the conversions follow the simpler/older pattern:

  if(${MODEL_TYPE} STREQUAL "asc")
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
      COMMAND asc2g "${CMAKE_CURRENT_SOURCE_DIR}/${in_model}" ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
      DEPENDS asc2g "${CMAKE_CURRENT_SOURCE_DIR}/${in_model}"
      )
  endif(${MODEL_TYPE} STREQUAL "asc")

  if(${MODEL_TYPE} STREQUAL "comgeom")
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
      COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
      COMMAND comgeom-g -v 5 ${CMAKE_CURRENT_SOURCE_DIR}/${in_model} ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file} > ${log_file} 2>&1
      DEPENDS comgeom-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )
  endif(${MODEL_TYPE} STREQUAL "comgeom")

  if(${MODEL_TYPE} STREQUAL "fg4")
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
      COMMAND fast4-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model} ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file} > ${log_file} 2>&1
      DEPENDS fast4-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )
  endif(${MODEL_TYPE} STREQUAL "fg4")

  if(${MODEL_TYPE} STREQUAL "stp")
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
      COMMAND step-g -O ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file} ${CMAKE_CURRENT_SOURCE_DIR}/${in_model} > ${log_file} 2>&1
      DEPENDS step-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )
  endif(${MODEL_TYPE} STREQUAL "stp")

  add_custom_target(${in_model_root}.g ALL DEPENDS ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file})
  set_target_properties(${in_model_root}.g PROPERTIES FOLDER "${folder}")

  # Install logic
  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
    install(FILES ${CMAKE_CURRENT_BUILD_DIR_INSTALL}/${output_file} DESTINATION ${DATA_DIR}/${rel_dir})
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)

  # Clean-up
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${log_file}")
  DISTCLEAN("${log_file}")

endfunction(ADD_G_TARGET in_model folder)

set(G_BENCHMARK_MODELS
  bldg391.asc
  m35.asc
  moss.asc
  sphflake.asc
  star.asc
  world.asc
)

set(G_SAMPLE_MODELS
  aet.asc
  axis.asc
  boolean-ops.asc
  castle.asc
  cornell.asc
  cornell-kunigami.asc
  cray.asc
  crod.asc
  cube.asc
  demo.asc
  die.asc
  galileo.asc
  goliath.asc
  havoc.asc
  kman.asc
  ktank.asc
  lgt-test.asc
  operators.asc
  pic.asc
  pinewood.asc
  prim.asc
  rounds.asc
  tank_car.asc
  terra.asc
  toyjeep.asc
  truck.asc
  wave.asc
  woodsman.asc
  xmp.asc
)
add_subdirectory(chess)
add_subdirectory(comgeom)
add_subdirectory(nist)
add_subdirectory(faa)

foreach(g_input ${G_BENCHMARK_MODELS})
  ADD_G_TARGET(${g_input} "BRL-CAD Geometry Models/Benchmark")
endforeach(g_input ${G_BENCHMARK_MODELS})

foreach(g_input ${G_SAMPLE_MODELS})
  ADD_G_TARGET(${g_input} "BRL-CAD Geometry Models/Sample")
endforeach(g_input ${G_SAMPLE_MODELS})

if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
   BRLCAD_ADDDATA(terra.dsp db)
endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)

CMAKEFILES(
  ${G_BENCHMARK_MODELS}
  ${G_SAMPLE_MODELS}
  aet.rt
  cornell.rt
  cube.rt
  db.php
  include/ctype.inc
  include/debug.inc
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
