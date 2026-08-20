link_directories(${CMAKE_BINARY_DIR}/${LIB_DIR})

set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../step-g/ap203.exp)

generate_schema_inputs(${STEP_SCHEMA_FILE} gstep ap203)

set(
  gstep_SOURCES
  ${gstep_express_srcs}
  ../BRLCADWrapper.cpp
  ../STEPMechanicalExport.cpp
  ../STEPConfigurationExport.cpp
  ../STEPConfigurationIdentityExport.cpp
  ../STEPExportMetadata.cpp
  ../STEPString.cpp
  ../ap203/AP203ConfigurationExport.cpp
  ../ap203/AP203ManagementExport.cpp
  AP203.cpp
  Assembly_Product.cpp
  Comb.cpp
  Default_Geometric_Context.cpp
  G_Objects.cpp
  ON_NurbsCurve.cpp
  ON_NurbsSurface.cpp
  ON_Brep.cpp
  Shape_Definition_Representation.cpp
  Shape_Representation.cpp
  Shape_Representation_Relationship.cpp
  Trees.cpp
  g-step.cpp
)
cmakefiles(${gstep_SOURCES})

if(BRLCAD_ENABLE_STEP)
  step_record_schema_sources(AP203 EXPORT gstep_SOURCES)
  set(
    gstep_LIBS
    libwdb
    librt
    libbrep
    libbn
    libbu
    ${gstep_SCHEMA_BINDINGS_TARGET}
    ${STEPCODE_UTILS_LIBRARY}
    ${STEPCODE_EDITOR_LIBRARY}
    ${STEPCODE_DAI_LIBRARY}
    ${STEPCODE_CORE_LIBRARY}
    OPENNURBS::OPENNURBS
  )

  set(
    gstep_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${STEPCODE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
  )

  brlcad_addexec(g-step "../StepExportHost.cpp" "libbu" NO_STRICT FOLDER Conv)
  target_include_directories(g-step BEFORE PRIVATE ${gstep_INCLUDE_DIRS})

  brlcad_add_test(
    NAME g_step_valid_output
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_valid_output.cmake
  )

  brlcad_add_test(
    NAME g_step_native_csg
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DASC2G=$<TARGET_FILE:asc2g>
      -DSOURCE_DB_DIR=${CMAKE_SOURCE_DIR}/db
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_native_csg.cmake
  )

  brlcad_add_test(
    NAME g_step_coverage
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_coverage.cmake
  )

  brlcad_add_test(
    NAME g_step_brep_matrix
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DNIRT=$<TARGET_FILE:nirt>
      -DBREP_TRIMMED=$<TARGET_FILE:brep_trimmed>
      -DBREP_INVALID=$<TARGET_FILE:brep_invalid>
      -DBREP_COBB=$<TARGET_FILE:brep_cobb>
      -DVOID_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/../ap214/tests/ap214_brep_with_voids.stp
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_brep_matrix.cmake
  )

  brlcad_add_test(
    NAME g_step_units
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_units.cmake
  )

  brlcad_add_test(
    NAME g_step_metadata_roundtrip
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_metadata_roundtrip.cmake
  )

  brlcad_add_test(
    NAME g_step_imported_product_layering
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/../ap214/tests/ap214_extruded_face.stp
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_imported_product_layering.cmake
  )

  brlcad_add_test(
    NAME g_step_ap203_complex_retention
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_complex_effectivity.stp
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_ap203_complex_retention.cmake
  )

  brlcad_add_test(
    NAME g_step_ap203_complex_export
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_ap203_complex_export.cmake
  )

  brlcad_add_test(
    NAME g_step_configuration_identities
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_configuration_identities.cmake
  )

  brlcad_add_test(
    NAME g_step_configuration_provenance
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_configuration_provenance.cmake
  )

  brlcad_add_test(
    NAME g_step_configuration_categories
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_configuration_categories.cmake
  )

  brlcad_add_test(
    NAME g_step_configuration_addresses
    COMMAND
      ${CMAKE_COMMAND}
      -DG_STEP=$<TARGET_FILE:g-step>
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/g_step_configuration_addresses.cmake
  )

  if(HIDE_INTERNAL_SYMBOLS)
    set(
      gstep_IMPORTS
      BU_DLL_IMPORTS
      BN_DLL_IMPORTS
      RT_DLL_IMPORTS
      WDB_DLL_IMPORTS
      SC_CORE_DLL_IMPORTS
      SC_DAI_DLL_IMPORTS
      SC_EDITOR_DLL_IMPORTS
      SC_UTILS_DLL_IMPORTS
    )
    set_property(TARGET g-step APPEND PROPERTY COMPILE_DEFINITIONS "${gstep_IMPORTS}")
  endif(HIDE_INTERNAL_SYMBOLS)

  # On some platforms distclean has additional cleanup to do
  distclean("${CMAKE_BINARY_DIR}/${LIB_DIR}/g-step.exp")
  distclean("${CMAKE_BINARY_DIR}/${LIB_DIR}/g-step.lib")

endif(BRLCAD_ENABLE_STEP)

set(
  IGNORE_FILES
  CMakeLists.txt
  AP203.h
  Assembly_Product.h
  Comb.h
  Default_Geometric_Context.h
  G_Objects.h
  ON_Brep.h
  Shape_Definition_Representation.h
  Shape_Representation.h
  Shape_Representation_Relationship.h
  Trees.h
  tests/g_step_coverage.cmake
  tests/g_step_brep_matrix.cmake
  tests/g_step_valid_output.cmake
  tests/g_step_native_csg.cmake
  tests/g_step_units.cmake
  tests/g_step_metadata_roundtrip.cmake
  tests/g_step_imported_product_layering.cmake
  tests/ap203_complex_effectivity.stp
  tests/g_step_ap203_complex_retention.cmake
  tests/g_step_ap203_complex_export.cmake
  tests/g_step_configuration_identities.cmake
  tests/g_step_configuration_provenance.cmake
  tests/g_step_configuration_categories.cmake
  tests/g_step_configuration_addresses.cmake
  ../STEPExportMetadata.h
  ../STEPConfigurationExport.h
  ../ap203/AP203ConfigurationExport.h
  ../ap203/AP203ManagementExport.h
)
cmakefiles(${IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
