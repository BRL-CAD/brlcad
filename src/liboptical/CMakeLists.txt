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

IF(BRLCAD-ENABLE_OSL)

    message("[Kunigami] OSL was enabled. Will compile shader OSL...")
    
    ##########################################
    # Build OSL-renderer
    ##########################################

    # Find dependencies (TODO: build this altogether)
    include(FindOSL)

    # NOTE(boulos): Boost can only detect that you've disabled RTTI for
    # gcc >= 4.3. My poor mac doesn't have that.
    add_definitions("-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID -Wno-error -no-pedantic -DOSL_ENABLED")

    include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${OSL_INCLUDES} 
	${OPENIMAGEIO_INCLUDES})

    message("Oslexec: ${OSLEXEC_LIBRARY}")
    message("Oslcomp: ${OSLCOMP_LIBRARY}")
    message("Oslquery: ${OSLQUERY_LIBRARY}")

    set (OSLRT_SOURCES osl-renderer.cpp render_svc.cpp)
    #set (OSLRT_LIBS ${OSLEXEC_LIBRARY} ${OSLCOMP_LIBRARY} ${OSLQUERY_LIBRARY} ${OPENIMAGEIO_LIBRARY} ${Boost_LIBRARIES})

    message("OSLRT_SOURCES: ${OSLRT_SOURCES}")
    message("OSLRT_LIBRARIES: ${OSLRT_LIBS}")

   # BRLCAD_ADDLIB(osl-renderer "${OSLRT_SOURCES}" "${OSLRT_LIBS}")

    BRLCAD_ADDLIB(osl-renderer "${OSLRT_SOURCES}" "/Users/kunigami/dev/osl/dist/macosx/lib/liboslexec.dylib /Users/kunigami/dev/osl/dist/macosx/lib/liboslcomp.dylib /Users/kunigami/dev/osl/dist/macosx/lib/liboslquery.dylib /Users/kunigami/dev/oiio/dist/macosx/lib/libOpenImageIO.dylib /opt/local/lib/libboost_filesystem-mt.dylib /opt/local/lib/libboost_regex-mt.dylib /opt/local/lib/libboost_system-mt.dylib /opt/local/lib/libboost_thread-mt.dylib")

    #BRLCAD_ADDLIB(osl-renderer "${OSLRT_SOURCES}" "/Users/kunigami/dev/osl/dist/macosx/lib/liboslexec.dylib;/Users/kunigami/dev/osl/dist/macosx/lib/liboslcomp.dylib;/Users/kunigami/dev/osl/dist/macosx/lib/liboslquery.dylib;/Users/kunigami/dev/oiio/dist/macosx/lib/libOpenImageIO.dylib;/opt/local/lib/libboost_filesystem-mt.dylib;/opt/local/lib/libboost_regex-mt.dylib;/opt/local/lib/libboost_system-mt.dylib;/opt/local/lib/libboost_thread-mt.dylib")

    ##########################################
    # OSL Shader 
    ##########################################

    # Add the osl shader to the list of shaders
    set(LIBOPTICAL_SOURCES ${LIBOPTICAL_SOURCES} sh_osl.c)

    # Link liboptical with osl-renderer library
    BRLCAD_ADDLIB(liboptical "${LIBOPTICAL_SOURCES}" "librt libbn libbu osl-renderer")

ELSE(BRLCAD-ENABLE_OSL)

    BRLCAD_ADDLIB(liboptical "${LIBOPTICAL_SOURCES}" "librt libbn libbu")

ENDIF(BRLCAD-ENABLE_OSL)

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
INSTALL(TARGETS librtms-static DESTINATION ${LIB_DIR})

CMAKEFILES(liboptical.dsp)
