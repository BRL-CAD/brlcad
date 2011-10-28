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

# Benchmark scripts don't know where the source directory is, so copy
# to the binary directory
FOREACH(pixfile ${BENCHMARK_PIX})
	configure_file(${pixfile} ${CMAKE_BINARY_DIR}/pix/${pixfile} COPYONLY)
ENDFOREACH(pixfile ${BENCHMARK_PIX})
FOREACH(pixlog ${PIX_LOG})
	configure_file(${pixlog} ${CMAKE_BINARY_DIR}/pix/${pixlog} COPYONLY)
ENDFOREACH(pixlog ${PIX_LOG})

FILE(COPY ${BENCHMARK_PIX} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/pix)
FILE(COPY ${PIX_LOG} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/pix)
INSTALL(FILES ${BENCHMARK_PIX} DESTINATION ${DATA_DIR}/pix)
INSTALL(FILES ${PIX_LOG} DESTINATION ${DATA_DIR}/pix)

CMAKEFILES(${BENCHMARK_PIX})
CMAKEFILES(${OTHER_PIX})
CMAKEFILES(${PIX_LOG})
CMAKEFILES(${SAVEVIEWS})
CMAKEFILES(Makefile.am)
