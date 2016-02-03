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

macro(ADD_NIST_STEP_TARGET step_model folder)
  string(REGEX REPLACE "([0-9a-z]*).stp" "\\1" step_model_root "${step_model}")
  set(output_file ${bin_root}/${DATA_DIR}/db/${step_model_root}.g)
  set(log_file ${CMAKE_CURRENT_BINARY_DIR}/${step_model_root}.log)
  add_custom_command(
    OUTPUT ${output_file}
    COMMAND step-g -O ${output_file} ${CMAKE_CURRENT_SOURCE_DIR}/${step_model} > ${log_file} 2>&1
    DEPENDS step-g ${CMAKE_CURRENT_SOURCE_DIR}/${step_model}
  )
  add_custom_target(${step_model_root}.g ALL DEPENDS ${output_file})
  set_target_properties(${step_model_root}.g PROPERTIES FOLDER "${folder}")
  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
    if(NOT "${CMAKE_CFG_INTDIR}" STREQUAL "." AND CMAKE_CONFIGURATION_FILES)
      string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" output_file "${output_file}")
    endif(NOT "${CMAKE_CFG_INTDIR}" STREQUAL "." AND CMAKE_CONFIGURATION_FILES)
    install(FILES ${output_file} DESTINATION ${DATA_DIR}/db)
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
  set(BUILT_MODELS "${BUILT_MODELS};${output_file}")
  set(LOG_FILES "${LOG_FILES};${log_file}")
  DISTCLEAN(${log_file})
endmacro(ADD_NIST_STEP_TARGET step_model folder)

macro(ADD_NIST_3DM_TARGET rhino_model folder)
  string(REGEX REPLACE "([0-9a-z]*).3dm" "\\1" rhino_model_root "${rhino_model}")
  set(output_file ${bin_root}/${DATA_DIR}/db/${rhino_model_root}.g)
  set(log_file ${CMAKE_CURRENT_BINARY_DIR}/${rhino_model_root}.log)
  add_custom_command(
    OUTPUT ${output_file}
    COMMAND 3dm-g -o ${output_file} ${CMAKE_CURRENT_SOURCE_DIR}/${rhino_model} > ${log_file} 2>&1
    DEPENDS 3dm-g ${CMAKE_CURRENT_SOURCE_DIR}/${rhino_model}
  )
  add_custom_target(${rhino_model_root}.g ALL DEPENDS ${output_file})
  set_target_properties(${rhino_model_root}.g PROPERTIES FOLDER "${folder}")
  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
    if(NOT "${CMAKE_CFG_INTDIR}" STREQUAL "." AND CMAKE_CONFIGURATION_FILES)
      string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" output_file "${output_file}")
    endif(NOT "${CMAKE_CFG_INTDIR}" STREQUAL "." AND CMAKE_CONFIGURATION_FILES)
    install(FILES ${output_file} DESTINATION ${DATA_DIR}/db)
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
  set(BUILT_MODELS "${BUILT_MODELS};${output_file}")
  set(LOG_FILES "${LOG_FILES};${log_file}")
  DISTCLEAN(${log_file})
endmacro(ADD_NIST_3DM_TARGET rhino_model folder)


foreach(step_input ${NIST_SAMPLE_STEP_MODELS})
  ADD_NIST_STEP_TARGET(${step_input} "BRL-CAD Geometry Models/Sample")
endforeach(step_input ${NIST_SAMPLE_STEP_MODELS})
CMAKEFILES(${NIST_SAMPLE_STEP_MODELS})

foreach(rhino_input ${NIST_SAMPLE_3DM_MODELS})
  ADD_NIST_3DM_TARGET(${rhino_input} "BRL-CAD Geometry Models/Sample")
endforeach(rhino_input ${NIST_SAMPLE_3DM_MODELS})
CMAKEFILES(${NIST_SAMPLE_3DM_MODELS})


set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${BUILT_MODELS}")
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${LOG_FILES}")

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
