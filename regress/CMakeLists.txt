# Geometry Conversion Regression Tests (experimental)
add_subdirectory(gcv)

if(SH_EXEC)
	add_custom_target(regress-mged
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/mged.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged
		)
	add_custom_target(regress-moss
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/moss.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt asc2g pixdiff asc2pix pix-png png-pix
		)
	add_custom_target(regress-lights
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/lights.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt asc2g pixdiff asc2pix
		)
	add_custom_target(regress-solids
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/solids.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix gencolor
		)
	add_custom_target(regress-shaders
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/shaders.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix gencolor
		)
	add_custom_target(regress-spdi
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/spdi.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt mged pixdiff asc2pix
		)
	add_custom_target(regress-iges
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/iges.sh ${CMAKE_SOURCE_DIR}
		DEPENDS iges-g g-iges mged
		)
	add_custom_target(regress-weight
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/weight.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rtweight mged
		)
	add_custom_target(regress-gqa
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/gqa.sh ${CMAKE_SOURCE_DIR}
		DEPENDS g_qa mged
		)
	add_custom_target(regress-fastgen
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/fastgen.sh ${CMAKE_SOURCE_DIR}
		DEPENDS fast4-g g_diff
		)
	add_custom_target(regress-bots
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/bots.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged pixdiff rt
		)
	add_custom_target(regress-flawfinder
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/flawfinder.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt
		)
	add_custom_target(regress-repository
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/repository.sh ${CMAKE_SOURCE_DIR}
		DEPENDS rt
		)
	add_custom_target(regress-red
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/red.sh ${CMAKE_SOURCE_DIR}
		DEPENDS mged
		)
	add_custom_target(regress-vls
		${SH_EXEC} ${CMAKE_SOURCE_DIR}/regress/vls.sh ${CMAKE_SOURCE_DIR}
		DEPENDS test_vls
		)
	add_custom_target(regress
		DEPENDS regress-mged regress-moss regress-lights regress-solids regress-shaders regress-spdi regress-iges regress-weight regress-gqa regress-fastgen regress-bots regress-flawfinder regress-repository regress-red regress-vls
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
