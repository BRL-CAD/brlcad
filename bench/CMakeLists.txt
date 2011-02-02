include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${TCL_INCLUDE_DIRS}
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
)

IF(MSVC)
	add_definitions(
		-DCONSOLE
		-DBRLCAD_DLL
		)
ENDIF(MSVC)

BRLCAD_ADDEXEC(pixcmp pixcmp.c libbu)

configure_file(run.sh ${CMAKE_BINARY_DIR}/bin/benchmark COPYONLY)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/bin/benchmark DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR})

SET(bench_MANS
  benchmark.1
  pixcmp.1
)
install(FILES ${bench_MANS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/man1)

IF(SH_EXEC)
	ADD_CUSTOM_TARGET(benchmark
		${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/benchmark run
		DEPENDS rt pixcmp
		)
	ADD_DEPENDENCIES(benchmark bldg391.g m35.g moss.g sphflake.g star.g world.g)
	ADD_CUSTOM_TARGET(benchmark-clean
		${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/benchmark clean
		)
	ADD_CUSTOM_TARGET(benchmark-clobber
		${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/benchmark clobber
		)
ENDIF(SH_EXEC)
