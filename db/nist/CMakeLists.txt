set(NIST_SAMPLE_STEP_MODELS
  NIST_MBE_PMI_1.stp
  NIST_MBE_PMI_2.stp
  NIST_MBE_PMI_3.stp
  NIST_MBE_PMI_4.stp
  NIST_MBE_PMI_5.stp
  NIST_MBE_PMI_6.stp
  NIST_MBE_PMI_11.stp
)
set(NIST_SAMPLE_3DM_MODELS
  NIST_MBE_PMI_7-10.3dm
  )

function(ADD_NIST_STEP_TARGET step_model folder)

  # Base output file name on input file
  string(REGEX REPLACE "([0-9a-z]*).stp" "\\1" step_model_root "${step_model}")

  # Define a relative output path - the quirks of CMake require this...
  set(output_file ${DATA_DIR}/db/${step_model_root}.g)

  # One log file per target
  set(log_file ${CMAKE_CURRENT_BINARY_DIR}/${step_model_root}.log)

  # Define the target
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
    COMMAND step-g -O ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file} ${CMAKE_CURRENT_SOURCE_DIR}/${step_model} > ${log_file} 2>&1
    DEPENDS step-g ${CMAKE_CURRENT_SOURCE_DIR}/${step_model}
    )
  add_custom_target(${step_model_root}.g ALL DEPENDS ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file})

  # Assign a target folder for tools like Visual Studio
  set_target_properties(${step_model_root}.g PROPERTIES FOLDER "${folder}")

  # If we're installing geometry, set up the install rule
  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
    install(FILES ${CMAKE_CURRENT_BUILD_DIR_INSTALL}/${output_file} DESTINATION ${DATA_DIR}/db)
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)

  # Cleanup
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file};${log_file}")
  DISTCLEAN(${log_file})

endfunction(ADD_NIST_STEP_TARGET step_model folder)

function(ADD_NIST_3DM_TARGET rhino_model folder)

  # Base output file name on input file
  string(REGEX REPLACE "([0-9a-z]*).3dm" "\\1" rhino_model_root "${rhino_model}")

  # Define a relative output path - the quirks of CMake require this...
  set(output_file ${DATA_DIR}/db/${rhino_model_root}.g)

  # Because we won't know the output directory in the script reliably until run time,
  # we append CURRENT_BUILD_DIR as a string to the arguments - it is the script's
  # responsibility to handle it correctly at run time.
  set(CMD_ARGS "-o" "CURRENT_BUILD_DIR/${output_file}" "${CMAKE_CURRENT_SOURCE_DIR}/${rhino_model}")

  # One log file per target - we capture both stdout and stderror here
  set(log_file ${CMAKE_CURRENT_BINARY_DIR}/${rhino_model_root}.log)

  # Use the model name for the script as well, since the output file is specific to this input
  set(script_file "${CMAKE_CURRENT_BINARY_DIR}/${rhino_model_root}.cmake")

  # We know enough - write the script
  generate_cmd_script(3dm-g "${script_file}" OLOG "${log_file}" ELOG "${log_file}" CARGS "${CMD_ARGS}")
  DISTCLEAN(${script_file})

  # Define the target
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file}
    COMMAND ${CMAKE_COMMAND} -P "${script_file}"
    DEPENDS 3dm-g ${CMAKE_CURRENT_SOURCE_DIR}/${rhino_model}
    )
  add_custom_target(${rhino_model_root}.g ALL DEPENDS ${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file})

  # Assign a target folder for tools like Visual Studio
  set_target_properties(${rhino_model_root}.g PROPERTIES FOLDER "${folder}")

  # If we're installing geometry, set up the install rule
  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
    install(FILES ${CMAKE_CURRENT_BUILD_DIR_INSTALL}/${output_file} DESTINATION ${DATA_DIR}/db)
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)

  # Cleanup
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BUILD_DIR_SCRIPT}/${output_file};${log_file}")
  DISTCLEAN(${log_file})

endfunction(ADD_NIST_3DM_TARGET rhino_model folder)


foreach(step_input ${NIST_SAMPLE_STEP_MODELS})
  ADD_NIST_STEP_TARGET(${step_input} "BRL-CAD Geometry Models/Sample")
endforeach(step_input ${NIST_SAMPLE_STEP_MODELS})
CMAKEFILES(${NIST_SAMPLE_STEP_MODELS})

foreach(rhino_input ${NIST_SAMPLE_3DM_MODELS})
  ADD_NIST_3DM_TARGET(${rhino_input} "BRL-CAD Geometry Models/Sample")
endforeach(rhino_input ${NIST_SAMPLE_3DM_MODELS})
CMAKEFILES(${NIST_SAMPLE_3DM_MODELS})


CMAKEFILES(
  README
  pdf
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
