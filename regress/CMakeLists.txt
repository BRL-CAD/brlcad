
IF(SH_EXEC)
	ADD_CUSTOM_TARGET(mged-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/mged.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged
		)
	ADD_CUSTOM_TARGET(moss-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/moss.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt asc2g pixdiff asc2pix pix-png png-pix
		)
	ADD_CUSTOM_TARGET(lights-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/lights.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt asc2g pixdiff asc2pix
		)
	ADD_CUSTOM_TARGET(solids-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/solids.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix gencolor
		)
	ADD_CUSTOM_TARGET(shaders-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/shaders.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix gencolor
		)
	ADD_CUSTOM_TARGET(spdi-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/spdi.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix
		)
	ADD_CUSTOM_TARGET(iges-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/iges.sh ${CMAKE_SOURCE_DIR}
		DEPENDS iges-g g-iges mged
		)
	ADD_CUSTOM_TARGET(weight-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/weight.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rtweight mged
		)
	ADD_CUSTOM_TARGET(gqa-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/gqa.sh ${CMAKE_SOURCE_DIR}
		DEPENDS g_qa mged
		)
	ADD_CUSTOM_TARGET(fastgen-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/fastgen.sh ${CMAKE_SOURCE_DIR}
		DEPENDS fast4-g g_diff
		)
	ADD_CUSTOM_TARGET(bots-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/bots.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged pixdiff rt
		)
	ADD_CUSTOM_TARGET(flawfinder-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/flawfinder.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt
		)
	ADD_CUSTOM_TARGET(repository-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/repository.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt
		)
	ADD_CUSTOM_TARGET(red-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/red.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged
		)
	ADD_CUSTOM_TARGET(regress
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/mged.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/moss.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/lights.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/solids.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/shaders.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/spdi.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/iges.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/weight.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/gqa.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/fastgen.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/bots.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/flawfinder.sh ${CMAKE_SOURCE_DIR}
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/repository.sh ${CMAKE_SOURCE_DIR}
		#COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/red.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff fast4-g g_diff g_qa rtweight iges-g g-iges asc2pix gencolor pix-png png-pix
		)

ENDIF(SH_EXEC)

CMAKEFILES(
	bots.sh 
	fastgen.sh 
	fastgen_dos.fast4 
	flawfinder.sh 
	gqa.sh 
	iges.sh 
	library.sh 
	lights.sh 
	lights_ref.asc 
	main.sh 
	master_fetch.sh 
	master_prep.sh 
	mged 
	mged.sh 
	mged_test.sh 
	moss.sh 
	mosspix.asc 
	nightly.sh 
	red.sh 
	repository.sh 
	shaders.sh 
	shaderspix.asc 
	slave_build.sh 
	solids.sh 
	solidspix.asc 
	spdi.sh 
	spdipix.asc 
	testlib.c 
	weight.sh
	)
