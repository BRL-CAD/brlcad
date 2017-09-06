set(RTBIN_INCLUDE_DIRS
  ${OPTICAL_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if(BRLCAD_ENABLE_OPENCL)
  add_definitions(-DUSE_OPENCL)
  set(RT_INCLUDE_DIRS ${RT_INCLUDE_DIRS} ${OPENCL_INCLUDE_DIRS})
endif (BRLCAD_ENABLE_OPENCL)

LIST(REMOVE_DUPLICATES RTBIN_INCLUDE_DIRS)
include_directories(${RTBIN_INCLUDE_DIRS})

set(LIBRTUIF_SOURCES
  do.c
  heatgraph.c
  main.c
  opt.c
  scanline.c
  worker.c
  )

# librtuif is used by the rt programs
add_library(librtuif OBJECT ${LIBRTUIF_SOURCES})

# TODO - Not used by anything??
add_library(librtuif_multispectral STATIC ${LIBRTUIF_SOURCES})
set_property(TARGET librtuif_multispectral APPEND PROPERTY LINKER_LANGUAGE C)
set_property(TARGET librtuif_multispectral APPEND PROPERTY COMPILE_DEFINITIONS "RT_MULTISPECTRAL")

if(BRLCAD_ENABLE_OPENCL)
  set(OPENCL_LIBS ${OPENCL_LIBRARIES})
endif(BRLCAD_ENABLE_OPENCL)

BRLCAD_ADDDATA(rtexample.c sample_applications)

BRLCAD_ADDEXEC(rtexample "rtexample.c" "librt;libfb;liboptical;libicv" NO_INSTALL)

BRLCAD_ADDEXEC(reshoot "reshoot.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv")

BRLCAD_ADDEXEC(rtwalk "rtwalk.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtshot "rtshot.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtregis "read-rtlog.c;rtregis.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv")

BRLCAD_ADDEXEC(rtscale "read-rtlog.c;rtscale.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rt "$<TARGET_OBJECTS:librtuif>;view.c" "${OPENCL_LIBS};${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rt_bot_faces "$<TARGET_OBJECTS:librtuif>;view_bot_faces.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rt_bot_faces APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtarea "$<TARGET_OBJECTS:librtuif>;viewarea.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtarea APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcell "$<TARGET_OBJECTS:librtuif>;viewcell.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcell APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcheck "$<TARGET_OBJECTS:librtuif>;viewcheck.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcheck APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtedge "$<TARGET_OBJECTS:librtuif>;viewedge.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtfrac "$<TARGET_OBJECTS:librtuif>;viewfrac.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtfrac APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtg3 "$<TARGET_OBJECTS:librtuif>;viewg3.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtg3 APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rthide "$<TARGET_OBJECTS:librtuif>;viewhide.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rthide APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtpp "$<TARGET_OBJECTS:librtuif>;viewpp.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtrad "$<TARGET_OBJECTS:librtuif>;viewrad.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrad APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtrange "$<TARGET_OBJECTS:librtuif>;viewrange.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrange APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtray "$<TARGET_OBJECTS:librtuif>;viewray.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtray APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtsil "$<TARGET_OBJECTS:librtuif>;viewsil.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtweight "$<TARGET_OBJECTS:librtuif>;viewweight.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtweight APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtxray "$<TARGET_OBJECTS:librtuif>;viewxray.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

set(rt_ignore_files
  ext.h
  libfb-dummy.c
  main.c
  rad.h
  rtuif.h
  scanline.h
  viewdir.c
  viewdummy.c
  viewscat.c
  )
CMAKEFILES(${rt_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
