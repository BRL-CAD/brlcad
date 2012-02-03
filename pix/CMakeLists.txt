set(BENCHMARK_PIX
  bldg391.pix
  m35.pix
  moss.pix
  sphflake.pix
  star.pix
  world.pix
)

set(OTHER_PIX
  golf_ball.pix
  lgt1.pix
  lgt2.pix
  lgt3.pix
  mworld.pix
  toilet.pix
)

set(PIX_LOG
  bldg391.log
  m35.log
  moss.log
  sphflake.log
  star.log
  world.log
)

set(SAVEVIEWS
  cube.rt
)

file(COPY ${BENCHMARK_PIX} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/pix)
file(COPY ${PIX_LOG} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/pix)
install(FILES ${BENCHMARK_PIX} DESTINATION ${DATA_DIR}/pix)
install(FILES ${PIX_LOG} DESTINATION ${DATA_DIR}/pix)
# Benchmark scripts don't know where the source directory is, so copy
# to the binary directory
foreach(pixfile ${BENCHMARK_PIX})
	if(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	  configure_file(${pixfile} ${CMAKE_BINARY_DIR}/pix/${pixfile} COPYONLY)
	endif(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	DISTCLEAN(${CMAKE_BINARY_DIR}/${DATA_DIR}/pix/${pixfile})
endforeach(pixfile ${BENCHMARK_PIX})
foreach(pixlog ${PIX_LOG})
	if(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	  configure_file(${pixlog} ${CMAKE_BINARY_DIR}/pix/${pixlog} COPYONLY)
	endif(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	DISTCLEAN(${CMAKE_BINARY_DIR}/${DATA_DIR}/pix/${pixlog})
endforeach(pixlog ${PIX_LOG})

CMAKEFILES(${BENCHMARK_PIX})
CMAKEFILES(${OTHER_PIX})
CMAKEFILES(${PIX_LOG})
CMAKEFILES(${SAVEVIEWS})
CMAKEFILES(Makefile.am)
