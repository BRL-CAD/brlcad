set(NIST_SAMPLE_MODELS
  NIST_MBE_PMI_1.stp
  NIST_MBE_PMI_2.stp
  NIST_MBE_PMI_3.stp
  NIST_MBE_PMI_4.stp
  NIST_MBE_PMI_5.stp
)

macro(ADD_NIST_G_TARGET step_model folder)
  string(REGEX REPLACE "([0-9a-z]*).stp" "\\1" step_model_root "${step_model}")
  set(output_file ${bin_root}/${DATA_DIR}/db/${step_model_root}.g)
  add_custom_command(
    OUTPUT ${output_file}
    COMMAND step-g -O ${output_file} ${CMAKE_CURRENT_SOURCE_DIR}/${step_model}
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
endmacro(ADD_NIST_G_TARGET step_model folder)

foreach(step_input ${NIST_SAMPLE_MODELS})
  ADD_NIST_G_TARGET(${step_input} "BRL-CAD Geometry Models/Sample")
endforeach(step_input ${NIST_SAMPLE_MODELS})
CMAKEFILES(${NIST_SAMPLE_MODELS})

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${BUILT_MODELS}")

CMAKEFILES(
  README
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
