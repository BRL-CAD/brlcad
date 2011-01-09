set(LIBOPTICAL_SOURCES
	init.c
	material.c
	photonmap.c
	refract.c
	sh_air.c
	sh_billboard.c
	sh_brdf.c
	sh_camo.c
	sh_cloud.c
	sh_cook.c
	sh_fbm.c
	sh_fire.c
	sh_flat.c
	sh_gauss.c
	sh_grass.c
	sh_light.c
	sh_noise.c
	sh_null.c
	sh_plastic.c
	sh_points.c
	sh_prj.c
	sh_rtrans.c
	sh_scloud.c
	sh_spm.c
	sh_stack.c
	sh_stxt.c
	sh_tcl.c
	sh_text.c
	sh_toon.c
	sh_toyota.c
	sh_treetherm.c
	sh_wood.c
	sh_xxx.c
	shade.c
	turb.c
	vers.c
	wray.c
)

include_directories(
    ${PNG_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
    ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDLIB(liboptical "${LIBOPTICAL_SOURCES}" "librt libbn libbu")
SET_TARGET_PROPERTIES(liboptical PROPERTIES VERSION 19.0.1 SOVERSION 19)

SET(LIBRTMS_SRCS
	sh_camo.c
	sh_light.c
	sh_noise.c
	sh_plastic.c
	sh_stack.c
	shade.c
	)

add_library(librtms-static STATIC ${LIBRTMS_SRCS})
target_link_libraries(librtms-static librt libbn libbu)
SET_TARGET_PROPERTIES(librtms-static PROPERTIES COMPILE_DEFINITIONS "RT_MULTISPECTRAL")
IF(NOT WIN32)
	SET_TARGET_PROPERTIES(librtms-static PROPERTIES OUTPUT_NAME "librtms")
ENDIF(NOT WIN32)
IF(WIN32)
	SET_TARGET_PROPERTIES(librtms-static PROPERTIES PREFIX "lib")
ENDIF(WIN32)
INSTALL(TARGETS librtms-static DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

