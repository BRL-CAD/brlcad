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
    ${TCL_INCLUDE_PATH}
)

BRLCAD_ADDLIB(liboptical "${LIBOPTICAL_SOURCES}" "libbu libbn librt")
SET_TARGET_PROPERTIES(liboptical PROPERTIES VERSION 19.0.1 SOVERSION 19)
