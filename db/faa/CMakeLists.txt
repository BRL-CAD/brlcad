set(FAA_SAMPLE_MODELS
  Business_Jet.fg4
  Generic_Twin.fg4
  )

macro(ADD_FAA_G_TARGET faa_model folder)
  string(REGEX REPLACE "([0-9a-z]*).fg4" "\\1" faa_model_root "${faa_model}")
  set(output_file ${bin_root}/${DATA_DIR}/db/${faa_model_root}.g)
  set(log_file ${CMAKE_CURRENT_BINARY_DIR}/${faa_model_root}.log)
  add_custom_command(
    OUTPUT ${output_file}
    COMMAND fast4-g ${CMAKE_CURRENT_SOURCE_DIR}/${faa_model} ${output_file} > ${log_file} 2>&1
    DEPENDS fast4-g ${CMAKE_CURRENT_SOURCE_DIR}/${faa_model}
  )
  add_custom_target(${faa_model_root}.g ALL DEPENDS ${output_file})
  set_target_properties(${faa_model_root}.g PROPERTIES FOLDER "${folder}")
  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
    if(NOT "${CMAKE_CFG_INTDIR}" STREQUAL "." AND CMAKE_CONFIGURATION_FILES)
      string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" output_file "${output_file}")
    endif(NOT "${CMAKE_CFG_INTDIR}" STREQUAL "." AND CMAKE_CONFIGURATION_FILES)
    install(FILES ${output_file} DESTINATION ${DATA_DIR}/db)
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
  set(BUILT_MODELS "${BUILT_MODELS};${output_file}")
  set(LOG_FILES "${LOG_FILES};${log_file}")
  DISTCLEAN(${log_file})
endmacro(ADD_FAA_G_TARGET faa_model folder)

foreach(faa_input ${FAA_SAMPLE_MODELS})
  ADD_FAA_G_TARGET(${faa_input} "BRL-CAD Geometry Models/Sample")
endforeach(faa_input ${FAA_SAMPLE_MODELS})
CMAKEFILES(
  ${FAA_SAMPLE_MODELS}
  README
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${BUILT_MODELS}")
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${LOG_FILES}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
