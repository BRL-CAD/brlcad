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
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/bin/benchmark DESTINATION ${BIN_DIR})

FILE(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake "MESSAGE(\"---\")\n")
FILE(APPEND ${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake "MESSAGE(\"Run 'make benchmark-clean' in the bench directory to delete the logic\")\n")
FILE(APPEND ${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake "MESSAGE(\"and image files generated during the benchmark analysis.\")\n")

SET(bench_MANS
  benchmark.1
  pixcmp.1
)
ADD_MAN_PAGES(1 bench_MANS)
CMAKEFILES(lgt.sh run.sh try.sh viewdiff.sh)

IF(SH_EXEC)
	ADD_CUSTOM_TARGET(benchmark
		COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/benchmark run
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake
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
