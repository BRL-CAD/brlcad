# Include directories needed by liboptical users
set(OPTICAL_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(optical OPTICAL_INCLUDE_DIRS "")

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

if(BRLCAD_ENABLE_OSL)

  ##########################################
  # Build OSL-renderer Library
  ##########################################

  # Find dependencies (TODO: build this altogether)
  include(FindOSL)

  # NOTE(boulos): Boost can only detect that you've disabled RTTI for
  # gcc >= 4.3. My poor mac doesn't have that.
  add_definitions("-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID -DOSL_ENABLED")

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OSL_INCLUDES}
    ${OPENIMAGEIO_INCLUDES}
    # This is required for brep.h to find opennurbs.h
    # (TODO: better way to include this dir?)
    ../other/openNURBS/
    )

  message("Oslexec: ${OSLEXEC_LIBRARY}")
  message("Oslcomp: ${OSLCOMP_LIBRARY}")
  message("Oslquery: ${OSLQUERY_LIBRARY}")

  set (OSLRT_SOURCES liboslrend.cpp render_svc.cpp)

  #set (OSLRT_LIBS ${OSLEXEC_LIBRARY} ${OSLCOMP_LIBRARY} ${OSLQUERY_LIBRARY} ${OPENIMAGEIO_LIBRARY} ${Boost_LIBRARIES})
  message("OSLRT_SOURCES: ${OSLRT_SOURCES}")
  #find_package(Boost)

  # Try to find BOOST
  set (Boost_COMPONENTS filesystem regex system thread)
  find_package (Boost 1.46 REQUIRED
    COMPONENTS ${Boost_COMPONENTS})
  # If it was not found, check if BOOST_ROOT was set
  if(NOT Boost_FILESYSTEM_LIBRARY)
    message("Cheking if BOOST ROOT was FOUND")
  endif()

  message("Boost filesystem: ${Boost_FILESYSTEM_LIBRARY}")
  message("Boost regex: ${Boost_REGEX_LIBRARY}")
  message("Boost system: ${Boost_SYSTEM_LIBRARY}")
  message("Boost thread: ${Boost_THREAD_LIBRARY}")

  set(OSLRT_LIBS "${OSLEXEC_LIBRARY} ${OSLCOMP_LIBRARY} ${OSLQUERY_LIBRARY} ${OPENIMAGEIO_LIBRARY} ${Boost_LIBRARIES} ${Boost_FILESYSTEM_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY}")

  string(REGEX REPLACE ";" " " OSLRT_LIBS2 "${OSLRT_LIBS}")

  BRLCAD_ADDLIB(liboslrend "${OSLRT_SOURCES}" "${OSLRT_LIBS2}")

  ##########################################
  # Build OSL Shader
  ##########################################

  # Add the osl shader to the list of shaders
  set(LIBOPTICAL_SOURCES ${LIBOPTICAL_SOURCES} sh_osl.cpp)

  # Link liboptical with osl-renderer library
  BRLCAD_ADDLIB(liboptical "${LIBOPTICAL_SOURCES}" "librt libbn libbu liboslrend")

else(BRLCAD_ENABLE_OSL)

  BRLCAD_ADDLIB(liboptical "${LIBOPTICAL_SOURCES}" "librt libbn libbu")

endif(BRLCAD_ENABLE_OSL)

CMAKEFILES(
  liboslrend.cpp
  oslexec_pvt.h
  render_svc.h
  sh_osl.cpp
  liboslrend.h
  constantpool.h
  render_svc.cpp
  )

SET_TARGET_PROPERTIES(liboptical PROPERTIES VERSION 19.0.1 SOVERSION 19)

set(LIBRTMS_SRCS
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
if(NOT WIN32)
  SET_TARGET_PROPERTIES(librtms-static PROPERTIES OUTPUT_NAME "librtms")
endif(NOT WIN32)
if(WIN32)
  SET_TARGET_PROPERTIES(librtms-static PROPERTIES PREFIX "lib")
endif(WIN32)
install(TARGETS librtms-static DESTINATION ${LIB_DIR})
CMAKEFILES(Makefile.am)
