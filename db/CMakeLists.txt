set(G_BENCHMARK_MODELS
  bldg391.asc
  m35.asc
  moss.asc
  sphflake.asc
  star.asc
  world.asc
)

set(G_SAMPLE_MODELS
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

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATA_DIR}/db)
foreach(g_model ${G_SAMPLE_MODELS})
  string(REGEX REPLACE "([0-9a-z]*).asc" "\\1" g_model_root "${g_model}")
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g
    COMMAND asc2g ${CMAKE_CURRENT_SOURCE_DIR}/${g_model} ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g
	 DEPENDS asc2g ${CMAKE_CURRENT_SOURCE_DIR}/${g_model}
  )
  add_custom_target(${g_model_root}.g ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g)
  if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
	  install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g DESTINATION ${DATA_DIR}/db)
  endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
  set(BUILT_MODELS "${BUILT_MODELS};${CMAKE_BINARY_DIR}/${DATA_DIR}/db/${g_model_root}.g")
endforeach(g_model ${G_SAMPLE_MODELS})
CMAKEFILES(${G_SAMPLE_MODELS})

SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${BUILT_MODELS}")

if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
	BRLCAD_ADDDATA(terra.dsp db)
endif(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
CMAKEFILES(cornell.rt db.php include)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
