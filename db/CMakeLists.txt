SET(G_BENCHMARK_MODELS
  bldg391.asc
  m35.asc
  moss.asc
  sphflake.asc
  star.asc
  world.asc
)

SET(G_SAMPLE_MODELS
  ${G_BENCHMARK_MODELS}
  axis.asc
  boolean-ops.asc
  castle.asc
  cornell.asc
  cornell-kunigami.asc
  cray.asc
  crod.asc
  cube.asc
  demo.asc
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
  tank_car.asc
  terra.asc
  toyjeep.asc
  truck.asc
  wave.asc
  woodsman.asc
  xmp.asc
)

FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATA_DIR}/db)
FOREACH(g_model ${G_SAMPLE_MODELS})
  STRING(REGEX REPLACE "([0-9a-z]*).asc" "\\1" g_model_root "${g_model}")
  ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g
    COMMAND asc2g ${CMAKE_CURRENT_SOURCE_DIR}/${g_model} ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g
	 DEPENDS asc2g ${CMAKE_CURRENT_SOURCE_DIR}/${g_model}
  )
  ADD_CUSTOM_TARGET(${g_model_root}.g ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g)
  IF(BRLCAD-INSTALL_EXAMPLE_GEOMETRY)
	  INSTALL(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g DESTINATION ${DATA_DIR}/db)
  ENDIF(BRLCAD-INSTALL_EXAMPLE_GEOMETRY)
  SET(BUILT_MODELS "${BUILT_MODELS};${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g")
ENDFOREACH(g_model ${G_SAMPLE_MODELS})
CMAKEFILES(${G_SAMPLE_MODELS})

SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${BUILT_MODELS}")

IF(BRLCAD-INSTALL_EXAMPLE_GEOMETRY)
	BRLCAD_ADDFILE(terra.dsp db)
ENDIF(BRLCAD-INSTALL_EXAMPLE_GEOMETRY)
CMAKEFILES(Makefile.am cornell.rt db.php include)
