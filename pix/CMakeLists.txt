SET(BENCHMARK_PIX
  bldg391.pix 
  m35.pix 
  moss.pix 
  sphflake.pix 
  star.pix 
  world.pix
)

SET(OTHER_PIX
  golf_ball.pix 
  lgt1.pix 
  lgt2.pix 
  lgt3.pix 
  mworld.pix 
  toilet.pix
)

SET(PIX_LOG
  bldg391.log 
  m35.log 
  moss.log 
  sphflake.log 
  star.log 
  world.log
)

SET(SAVEVIEWS
  cube.rt
)

INSTALL(FILES ${BENCHMARK_PIX} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/pix)
INSTALL(FILES ${PIX_LOG} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/pix)
