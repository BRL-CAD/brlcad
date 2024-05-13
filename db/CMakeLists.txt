function(G_TARGET in_model folder)
  # Initialize
  set(MODEL_TYPE "UNKNOWN")

  # Regardless of what we decide, let CMake know about the file
  CMAKEFILES(${CMAKE_CURRENT_SOURCE_DIR}/${in_model})

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

  # Make sure variables are set up for configure_file
  set(in_model_root "${in_model_root}")

  # Find our output file and path
  string(REPLACE "${CMAKE_SOURCE_DIR}/" "" rel_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  set(output_file ${DATA_DIR}/${rel_dir}/${in_model_root}.g)
  set(log_file ${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.log)

  # Make sure the output db target directory is there
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${DATA_DIR}/${rel_dir}")

  # Rhino 3DM file
  if(${MODEL_TYPE} STREQUAL "3dm")
    if (NOT TARGET 3dm-g)
      return()
    endif (NOT TARGET 3dm-g)
    set(CONV_ARGS "-o ${CMAKE_BINARY_DIR}/${output_file} ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}")
    configure_file("${CMAKE_SOURCE_DIR}/db/to_g.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake" @ONLY)
    # Custom command to generate the output
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/${output_file}
      COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/${output_file}"
      COMMAND ${CMAKE_COMMAND} -DEXEC=$<TARGET_FILE:3dm-g> -P "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake"
      DEPENDS 3dm-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )
  endif(${MODEL_TYPE} STREQUAL "3dm")

  # BRL-CAD ASCII database
  if(${MODEL_TYPE} STREQUAL "asc")
    set(CONV_ARGS "${CMAKE_CURRENT_SOURCE_DIR}/${in_model} ${CMAKE_BINARY_DIR}/${output_file} ")
    configure_file("${CMAKE_SOURCE_DIR}/db/to_g.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake" @ONLY)
    if (BRLCAD_ENABLE_TCL)
      if (NOT TARGET asc2g)
	return()
      endif (NOT TARGET asc2g)
      # Custom command to generate the output
      add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/${output_file}
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${output_file}
	COMMAND ${CMAKE_COMMAND} -DEXEC=$<TARGET_FILE:asc2g> -P "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake"
	DEPENDS asc2g "${CMAKE_CURRENT_SOURCE_DIR}/${in_model}"
	)
    else (BRLCAD_ENABLE_TCL)
      if (NOT TARGET gcv)
	return()
      endif (NOT TARGET gcv)
      # Custom command to generate the output
      add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/${output_file}
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${output_file}
	COMMAND ${CMAKE_COMMAND} -DEXEC=$<TARGET_FILE:gcv> -P "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake"
	DEPENDS gcv "${CMAKE_CURRENT_SOURCE_DIR}/${in_model}"
	)
    endif (BRLCAD_ENABLE_TCL)
  endif(${MODEL_TYPE} STREQUAL "asc")

  # COMGEOM (pre BRL-CAD Geometry Information For Targets (GIFT) files)
  if(${MODEL_TYPE} STREQUAL "comgeom")
    if (NOT TARGET comgeom-g)
      return()
    endif (NOT TARGET comgeom-g)
    set(CONV_ARGS "-v 5 ${CMAKE_CURRENT_SOURCE_DIR}/${in_model} ${CMAKE_BINARY_DIR}/${output_file}")
    configure_file("${CMAKE_SOURCE_DIR}/db/to_g.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake" @ONLY)
    # Custom command to generate the output
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/${output_file}
      COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/${output_file}"
      COMMAND ${CMAKE_COMMAND} -DEXEC=$<TARGET_FILE:comgeom-g> -P "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake"
      DEPENDS comgeom-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )
  endif(${MODEL_TYPE} STREQUAL "comgeom")

  # FASTGEN 4
  if(${MODEL_TYPE} STREQUAL "fg4")
    if (NOT TARGET fast4-g)
      return()
    endif (NOT TARGET fast4-g)
    set(CONV_ARGS "${CMAKE_CURRENT_SOURCE_DIR}/${in_model} ${CMAKE_BINARY_DIR}/${output_file}")
    configure_file("${CMAKE_SOURCE_DIR}/db/to_g.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake" @ONLY)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/${output_file}
      COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/${output_file}"
      COMMAND ${CMAKE_COMMAND} -DEXEC=$<TARGET_FILE:fast4-g> -P "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake"
      DEPENDS fast4-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )
  endif(${MODEL_TYPE} STREQUAL "fg4")

  # STEP AP203
  if(${MODEL_TYPE} STREQUAL "stp")
    if (NOT TARGET step-g)
      return()
    endif (NOT TARGET step-g)
    set(CONV_ARGS "-O ${CMAKE_BINARY_DIR}/${output_file} ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}")
    configure_file("${CMAKE_SOURCE_DIR}/db/to_g.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake" @ONLY)
    # Custom command to generate the output
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/${output_file}
      COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/${output_file}"
      COMMAND ${CMAKE_COMMAND} -DEXEC=$<TARGET_FILE:step-g> -P "${CMAKE_CURRENT_BINARY_DIR}/${in_model_root}.cmake"
      DEPENDS step-g ${CMAKE_CURRENT_SOURCE_DIR}/${in_model}
      )
  endif(${MODEL_TYPE} STREQUAL "stp")

  # If we got this far, do the target, install and clean-up rules
  add_custom_target(${in_model_root}.g ALL DEPENDS ${CMAKE_BINARY_DIR}/${output_file})
  set_target_properties(${in_model_root}.g PROPERTIES FOLDER "${folder}")

  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
    install(FILES ${CMAKE_BINARY_DIR}/${output_file} DESTINATION ${DATA_DIR}/${rel_dir})
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
  
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${log_file}")
  DISTCLEAN("${log_file}")

endfunction(G_TARGET in_model folder)

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
  annual_gift_man.asc
  axis.asc
  bearing.asc
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
  radialgrid.asc
  rounds.asc
  shipping_container.asc
  tank_car.asc
  terra.asc
  toyjeep.asc
  traffic_light.asc
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
  G_TARGET(${g_input} "BRL-CAD Geometry Models/Benchmark")
endforeach(g_input ${G_BENCHMARK_MODELS})

foreach(g_input ${G_SAMPLE_MODELS})
  G_TARGET(${g_input} "BRL-CAD Geometry Models/Sample")
endforeach(g_input ${G_SAMPLE_MODELS})

if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY AND TARGET gcv)
  BRLCAD_ADDDATA(terra.bin db)
endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY AND TARGET gcv)

if(SH_EXEC)
  if (TARGET mged OR TARGET gcv)
    set(conversion_g_files)
    set(conversion_g_files_fp)
    set(conversion_deps_list)

    macro(Sh_Conversion_Test gfile depends_list)
      if (TARGET mged)
	add_custom_target(convert-${gfile} COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/sh/conversion.sh GED=$<TARGET_FILE:mged> ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${gfile})
      else ()
	add_custom_target(convert-${gfile} COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/sh/conversion.sh GED=$<TARGET_FILE:gcv> ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${gfile})
      endif (TARGET mged)
      set_target_properties(convert-${gfile} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
      set_target_properties(convert-${gfile} PROPERTIES FOLDER "BRL-CAD Conversion Tests")
      set(conversion_g_files ${conversion_g_files} ${gfile})
      if(NOT "${depends_list}" STREQUAL "")
	add_dependencies(convert-${gfile} ${depends_list})
	set(conversion_deps_list ${conversion_deps_list} ${depends_list})
      endif(NOT "${depends_list}" STREQUAL "")
    endmacro(Sh_Conversion_Test)

    foreach(g_input ${G_BENCHMARK_MODELS})
      get_filename_component(GROOT "${g_input}" NAME_WE)
      if (TARGET mged)
	Sh_Conversion_Test(${GROOT}.g "mged")
      else ()
	Sh_Conversion_Test(${GROOT}.g "gcv")
      endif (TARGET mged)
    endforeach(g_input ${G_BENCHMARK_MODELS})
    foreach(g_input ${G_SAMPLE_MODELS})
      get_filename_component(GROOT "${g_input}" NAME_WE)
      if (TARGET mged)
	Sh_Conversion_Test(${GROOT}.g "mged")
      else ()
	Sh_Conversion_Test(${GROOT}.g "gcv")
      endif (TARGET mged)
    endforeach(g_input ${G_SAMPLE_MODELS})

    list(REMOVE_DUPLICATES conversion_deps_list)
    foreach(gfile ${conversion_g_files})
      set(conversion_g_files_fp ${conversion_g_files_fp} "${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${gfile}")
    endforeach(gfile ${conversion_g_files})
    if (TARGET mged)
      add_custom_target(convert COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/sh/conversion.sh GED=$<TARGET_FILE:mged> ${conversion_g_files_fp})
    else ()
      add_custom_target(convert COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/sh/conversion.sh GED=$<TARGET_FILE:gcv> ${conversion_g_files_fp})
    endif (TARGET mged)
    set_target_properties(convert PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
    set_target_properties(convert PROPERTIES FOLDER "BRL-CAD Conversion Tests")
    add_dependencies(convert ${conversion_deps_list})
  endif (TARGET mged OR TARGET gcv)
endif(SH_EXEC)

CMAKEFILES(
  ${G_BENCHMARK_MODELS}
  ${G_SAMPLE_MODELS}
  CMakeLists.txt
  aet.rt
  cornell.rt
  cube.rt
  traffic_light.rt
  db.php
  include/ctype.inc
  include/debug.inc
  terra.bin
  to_g.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
