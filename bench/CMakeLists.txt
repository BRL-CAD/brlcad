include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
)

BRLCAD_ADDEXEC(pixcmp pixcmp.c bu)

configure_file(run.sh ${CMAKE_CURRENT_BINARY_DIR}/benchmark COPYONLY)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/benchmark DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

SET(bench_MANS
  benchmark.1
  pixcmp.1
)
install(FILES ${bench_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)

IF(SH_EXEC)
ADD_CUSTOM_COMMAND(TARGET benchmark
  COMMAND ${SH_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/run.sh run
  DEPENDS rt bldg391.g m35.g moss.g sphflake.g star.g world.g
  )
ADD_CUSTOM_COMMAND(TARGET benchmark-clean
  COMMAND ${SH_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/run.sh clean
  DEPENDS rt bldg391.g m35.g moss.g sphflake.g star.g world.g
  )
ADD_CUSTOM_COMMAND(TARGET benchmark-clobber
  COMMAND ${SH_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/run.sh clobber
  DEPENDS rt bldg391.g m35.g moss.g sphflake.g star.g world.g
  )
ENDIF(SH_EXEC)
