# Geometry Conversion Regression Tests (experimental)
add_subdirectory(gcv)

if(SH_EXEC)
	add_custom_target(mged-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/mged.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged
		)
	add_custom_target(moss-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/moss.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt asc2g pixdiff asc2pix pix-png png-pix
		)
	add_custom_target(lights-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/lights.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt asc2g pixdiff asc2pix
		)
	add_custom_target(solids-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/solids.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix gencolor
		)
	add_custom_target(shaders-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/shaders.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix gencolor
		)
	add_custom_target(spdi-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/spdi.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix
		)
	add_custom_target(iges-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/iges.sh ${CMAKE_SOURCE_DIR}
		DEPENDS iges-g g-iges mged
		)
	add_custom_target(weight-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/weight.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rtweight mged
		)
	add_custom_target(gqa-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/gqa.sh ${CMAKE_SOURCE_DIR}
		DEPENDS g_qa mged
		)
	add_custom_target(fastgen-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/fastgen.sh ${CMAKE_SOURCE_DIR}
		DEPENDS fast4-g g_diff
		)
	add_custom_target(bots-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/bots.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged pixdiff rt
		)
	add_custom_target(flawfinder-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/flawfinder.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt
		)
	add_custom_target(repository-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/repository.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt
		)
	add_custom_target(red-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/red.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged
		)
	add_custom_target(vls-regress
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/vls.sh ${CMAKE_SOURCE_DIR}
		DEPENDS test_vls
		)
	add_custom_target(regress
		COMMAND ${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/vls.sh ${CMAKE_SOURCE_DIR}
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
		DEPENDS test_vls rt mged pixdiff fast4-g g_diff g_qa rtweight iges-g g-iges asc2pix gencolor pix-png png-pix
		)

endif(SH_EXEC)

CMAKEFILES(
	Makefile.am
	bots.sh 
        dsp.sh
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
	vls.sh
	weight.sh
	gcv
	)
